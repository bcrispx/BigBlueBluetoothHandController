package com.example.bigbluebluetoothhandcontroller

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.Log
import android.view.MotionEvent
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.example.bigbluebluetoothhandcontroller.databinding.ActivityMainBinding
import java.io.IOException
import java.io.OutputStream
import java.util.*

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding
    private var bluetoothSocket: BluetoothSocket? = null
    private var outputStream: OutputStream? = null
    private var isSpiralSearchActive = false
    private var spiralSpeed = 1

    private fun performHapticFeedback() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            if (vibrator.hasVibrator()) {
                vibrator.vibrate(VibrationEffect.createOneShot(50, VibrationEffect.DEFAULT_AMPLITUDE))
            }
        } else {
            @Suppress("DEPRECATION")
            val vibrator = getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
            vibrator.vibrate(50)
        }
    }

    companion object {
        private const val TAG = "MainActivity"
        private val ESP32_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
        private const val ESP32_NAME = "ESP32_Direction_Control"
        private const val SPIRAL_STEP_SIZE = 100 // Milliseconds per direction
        private const val SPIRAL_GROWTH = 1.5f // How much larger each spiral loop gets
    }

    private val bluetoothPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        if (permissions.all { it.value }) {
            setupBluetoothConnection()
        } else {
            Toast.makeText(this, "Bluetooth permissions required", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        try {
            super.onCreate(savedInstanceState)
            binding = ActivityMainBinding.inflate(layoutInflater)
            setContentView(binding.root)
            
            setupButtons()
            updateConnectionStatus(false)
        } catch (e: Exception) {
            Log.e(TAG, "Error in onCreate: ${e.message}")
            e.printStackTrace()
            Toast.makeText(this, "Error starting app: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun setupButtons() {
        binding.btnSpiralSpeed.setOnClickListener {
            spiralSpeed = if (spiralSpeed >= 3) 1 else spiralSpeed + 1
            binding.btnSpiralSpeed.text = "Speed: ${spiralSpeed}x"
            if (isSpiralSearchActive) {
                sendCommand("SPIRAL,SPEED,$spiralSpeed\n")
            }
        }

        binding.btnConnect.setOnClickListener {
            if (bluetoothSocket?.isConnected == true) {
                disconnectBluetooth()
            } else {
                checkBluetoothPermissions()
            }
        }

        val buttonCommands = mapOf(
            binding.btnNorth to "NORTH",
            binding.btnSouth to "SOUTH",
            binding.btnEast to "EAST",
            binding.btnWest to "WEST"
        )

        buttonCommands.forEach { (button, direction) ->
            button.setOnTouchListener { _, event ->
                when (event.action) {
                    MotionEvent.ACTION_DOWN -> {
                        if (isSpiralSearchActive) {
                            stopSpiralSearch()
                        } else {
                            sendCommand("$direction,START\n")
                        }
                    }
                    MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                        if (!isSpiralSearchActive) {
                            sendCommand("$direction,STOP\n")
                        }
                    }
                }
                true
            }
        }

        binding.btnSpiral.setOnClickListener {
            if (!isSpiralSearchActive) {
                startSpiralSearch()
            } else {
                stopSpiralSearch()
            }
        }
    }

    private fun checkBluetoothPermissions() {
        val requiredPermissions = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(
                Manifest.permission.BLUETOOTH_SCAN,
                Manifest.permission.BLUETOOTH_CONNECT
            )
        } else {
            arrayOf(
                Manifest.permission.BLUETOOTH,
                Manifest.permission.BLUETOOTH_ADMIN
            )
        }

        val missingPermissions = requiredPermissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missingPermissions.isEmpty()) {
            setupBluetoothConnection()
        } else {
            bluetoothPermissionLauncher.launch(missingPermissions.toTypedArray())
        }
    }

    private fun setupBluetoothConnection() {
        try {
            val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
            val bluetoothAdapter = bluetoothManager.adapter

            if (bluetoothAdapter == null) {
                Toast.makeText(this, "Bluetooth is not available", Toast.LENGTH_LONG).show()
                return
            }

            if (!bluetoothAdapter.isEnabled) {
                val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
                if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED) {
                    startActivity(enableBtIntent)
                }
                return
            }

            Thread {
                try {
                    if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                        return@Thread
                    }

                    val pairedDevices: Set<BluetoothDevice> = bluetoothAdapter.bondedDevices
                    val esp32Device = pairedDevices.find { device ->
                        device.name == ESP32_NAME
                    }

                    if (esp32Device == null) {
                        runOnUiThread {
                            Toast.makeText(this, "ESP32 device not found. Please pair first.", Toast.LENGTH_LONG).show()
                        }
                        return@Thread
                    }

                    bluetoothSocket = esp32Device.createRfcommSocketToServiceRecord(ESP32_UUID)
                    bluetoothSocket?.connect()
                    outputStream = bluetoothSocket?.outputStream

                    runOnUiThread {
                        updateConnectionStatus(true)
                        Toast.makeText(this, "Connected to ESP32", Toast.LENGTH_SHORT).show()
                    }

                } catch (e: IOException) {
                    Log.e(TAG, "Error connecting to ESP32: ${e.message}")
                    runOnUiThread {
                        Toast.makeText(this, "Failed to connect: ${e.message}", Toast.LENGTH_LONG).show()
                        updateConnectionStatus(false)
                    }
                    disconnectBluetooth()
                }
            }.start()

        } catch (e: Exception) {
            Log.e(TAG, "Error setting up Bluetooth: ${e.message}")
            Toast.makeText(this, "Error setting up Bluetooth: ${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    private fun sendCommand(command: String) {
        try {
            if (bluetoothSocket?.isConnected != true) {
                Log.e(TAG, "Not connected to device")
                return
            }

            outputStream?.write(command.toByteArray())
            outputStream?.flush()
            performHapticFeedback()
        } catch (e: Exception) {
            Log.e(TAG, "Error sending command: $command", e)
            e.printStackTrace()
        }
    }

    private fun disconnectBluetooth() {
        try {
            bluetoothSocket?.close()
        } catch (e: IOException) {
            Log.e(TAG, "Error closing socket: ${e.message}")
        } finally {
            bluetoothSocket = null
            outputStream = null
            updateConnectionStatus(false)
        }
    }

    private fun updateConnectionStatus(connected: Boolean) {
        binding.btnConnect.text = if (connected) "Disconnect" else "Connect to Big Blue"
        binding.btnNorth.isEnabled = connected
        binding.btnSouth.isEnabled = connected
        binding.btnEast.isEnabled = connected
        binding.btnWest.isEnabled = connected
        binding.btnSpiral.isEnabled = connected
        binding.btnSpiralSpeed.isEnabled = connected
    }

    private fun startSpiralSearch() {
        if (bluetoothSocket?.isConnected != true) return
        
        isSpiralSearchActive = true
        binding.btnSpiral.text = "Stop Spiral"
        sendCommand("SPIRAL,START,$spiralSpeed\n")
    }

    private fun stopSpiralSearch() {
        isSpiralSearchActive = false
        binding.btnSpiral.text = "Spiral Search"
        sendCommand("SPIRAL,STOP\n")
    }

    override fun onDestroy() {
        super.onDestroy()
        stopSpiralSearch()
        disconnectBluetooth()
    }
}
