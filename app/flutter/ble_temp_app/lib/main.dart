import 'dart:async';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:permission_handler/permission_handler.dart';
import 'dart:io';

void main() {
  runApp(const BLETempApp());
}

class BLETempApp extends StatelessWidget {
  const BLETempApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'BLE Temp Monitor',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(
          seedColor: const Color(0xFF00BCD4),
          brightness: Brightness.dark,
        ),
        useMaterial3: true,
      ),
      home: const TempMonitorScreen(),
    );
  }
}

// ─── UUIDs ────────────────────────────────────────────────────────────────────
const String _serviceUuid = "49535343-FE7D-4AE5-8FA9-9FAFD205E455";
const String _txUuid      = "49535343-1E4D-4BD9-BA61-23C647249616";
const String _rxUuid      = "49535343-8841-43F4-A8D4-ECBE34729BB3";

const List<int> _startCmd = [
  0x53,0x54,0x41,0x52,0x54,0x5F,0x54,0x45,0x4D,0x50,0x0D,0x0A // START_TEMP\r\n
];
const List<int> _stopCmd = [
  0x53,0x54,0x4F,0x50,0x5F,0x54,0x45,0x4D,0x50,0x0D,0x0A      // STOP_TEMP\r\n
];

// ─── States ───────────────────────────────────────────────────────────────────
enum AppState { disconnected, scanning, connecting, connected, streaming, error }

// ─── Screen ───────────────────────────────────────────────────────────────────
class TempMonitorScreen extends StatefulWidget {
  const TempMonitorScreen({super.key});
  @override
  State<TempMonitorScreen> createState() => _TempMonitorScreenState();
}

class _TempMonitorScreenState extends State<TempMonitorScreen> {
  AppState _appState = AppState.disconnected;
  String _deviceName = '';
  String _tempC      = '--';
  String _tempF      = '--';
  String _errorMsg   = '';
  bool _reconnecting = false;

  BluetoothDevice?         _device;
  BluetoothCharacteristic? _txChar;
  BluetoothCharacteristic? _rxChar;
  StreamSubscription?      _scanSub;
  StreamSubscription?      _isScanSub;
  StreamSubscription?      _connStateSub;
  StreamSubscription?      _notifySub;
  String _buffer = '';

  @override
  void dispose() {
    _scanSub?.cancel();
    _isScanSub?.cancel();
    _connStateSub?.cancel();
    _notifySub?.cancel();
    super.dispose();
  }

  // ─── BLE Flow ───────────────────────────────────────────────────────────────

Future<void> _startScan() async {
 // Android-only: request BLE permissions at runtime
if (Platform.isAndroid) {
  final bluetoothScan    = await Permission.bluetoothScan.request();
  final bluetoothConnect = await Permission.bluetoothConnect.request();

  if (bluetoothScan.isDenied || bluetoothConnect.isDenied) {
    setState(() {
      _appState = AppState.error;
      _errorMsg = 'Bluetooth permissions denied. Enable them in Settings → Apps → ble_temp_app → Permissions.';
    });
    return;
  }

  if (bluetoothScan.isPermanentlyDenied || bluetoothConnect.isPermanentlyDenied) {
    setState(() {
      _appState = AppState.error;
      _errorMsg = 'Bluetooth permissions permanently denied. Go to Settings → Apps → ble_temp_app → Permissions and enable Nearby Devices.';
    });
    openAppSettings();
    return;
  }
}
// iOS: permission granted at launch via NSBluetoothAlwaysUsageDescription prompt

  // Cancel and null previous subscriptions
  await _scanSub?.cancel();
  await _isScanSub?.cancel();
  _scanSub   = null;
  _isScanSub = null;

  await FlutterBluePlus.stopScan();

  setState(() {
    _appState   = AppState.scanning;
    _errorMsg   = '';
    _tempC      = '--';
    _tempF      = '--';
    _deviceName = '';
  });

  bool scanHasStarted = false;

  _scanSub = FlutterBluePlus.onScanResults.listen((results) async {
    if (results.isEmpty) return;
    final matches = results.where(
      (r) => r.device.advName.contains('RN4870') ||
             r.device.advName.contains('RN4871') ||
             r.advertisementData.serviceUuids.contains(Guid(_serviceUuid))
    );
    if (matches.isEmpty) return;
    final r = matches.first;
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
    await _isScanSub?.cancel();
    _scanSub   = null;
    _isScanSub = null;
    await _connectToDevice(r.device, r.advertisementData.advName);
  });

  _isScanSub = FlutterBluePlus.isScanning.listen((scanning) {
    if (scanning) {
      scanHasStarted = true;
      return;
    }
    if (scanHasStarted && _appState == AppState.scanning && mounted) {
      setState(() {
        _appState = AppState.error;
        _errorMsg = 'No device found. Is the ZedBoard powered and advertising?';
      });
    }
  });

  await FlutterBluePlus.startScan(
    timeout: const Duration(seconds: 10),
  );
}

  Future<void> _connectToDevice(BluetoothDevice device, String name) async {
    if (!mounted) return;
    setState(() {
      _appState   = AppState.connecting;
      _deviceName = name.isNotEmpty ? name : 'ZedBoard';
    });
    _device = device;

    try {
      await device.connect(license: License.nonprofit, timeout: const Duration(seconds: 15));
    } catch (e) {
      if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Connection failed: $e'; });
      return;
    }

    await _connStateSub?.cancel();
    _connStateSub = device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected) _handleDisconnect();
    });

    await _discoverAndSubscribe(device);
  }

  Future<void> _discoverAndSubscribe(BluetoothDevice device) async {
    if (!mounted) return;
    setState(() => _appState = AppState.connected);

    List<BluetoothService> services;
    try {
      services = await device.discoverServices();
    } catch (e) {
      if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Service discovery failed: $e'; });
      return;
    }

    BluetoothCharacteristic? txChar;
    BluetoothCharacteristic? rxChar;
    for (final s in services) {
      for (final c in s.characteristics) {
        if (c.characteristicUuid == Guid(_txUuid)) txChar = c;
        if (c.characteristicUuid == Guid(_rxUuid)) rxChar = c;
      }
    }

    if (txChar == null || rxChar == null) {
      if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Required characteristics not found.'; });
      return;
    }

    _txChar = txChar;
    _rxChar = rxChar;
    _buffer = '';
    if (mounted) setState(() => _errorMsg = '');

    try {
      await txChar.setNotifyValue(true);
    } catch (e) {
      if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Failed to subscribe to notifications: $e'; });
      return;
    }

    _notifySub = txChar.onValueReceived.listen(_handleIncoming);
    device.cancelWhenDisconnected(_notifySub!);

    await _sendStartTemp();
  }

  Future<void> _sendStartTemp() async {
    if (_rxChar == null) return;
    try {
      await _rxChar!.write(_startCmd, withoutResponse: false);
      if (mounted) setState(() => _appState = AppState.streaming);
    } catch (e) {
      if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Failed to send START_TEMP: $e'; });
    }
  }

  Future<void> _sendStopTemp() async {
    if (_rxChar == null) return;
    try { await _rxChar!.write(_stopCmd, withoutResponse: false); } catch (_) {}
  }

  // ─── Disconnect / Reconnect ─────────────────────────────────────────────────

  void _handleDisconnect() async {
    await _notifySub?.cancel();
    _notifySub = null;
    _txChar    = null;
    _rxChar    = null;
    _buffer    = '';

    if (_reconnecting) return;

    if (mounted) setState(() { _appState = AppState.disconnected; _tempC = '--'; _tempF = '--'; _reconnecting = true; });

    await Future.delayed(const Duration(seconds: 2));

    if (_device != null) {
      try {
        await _device!.connect(license: License.nonprofit, timeout:  const Duration(seconds: 15));
        _reconnecting = false;
        await _discoverAndSubscribe(_device!);
      } catch (_) {
        _reconnecting = false;
        if (mounted) setState(() { _appState = AppState.error; _errorMsg = 'Reconnect failed. Tap Retry to try again.'; });
      }
    }
  }

  Future<void> _disconnect() async {
    await _sendStopTemp();
    await _connStateSub?.cancel();
    _connStateSub = null;
    await _notifySub?.cancel();
    _notifySub    = null;
    await _device?.disconnect();
    _device       = null;
    _txChar       = null;
    _rxChar       = null;
    _buffer       = '';
    _reconnecting = false;
    if (mounted) {
      setState(() {
      _appState   = AppState.disconnected;
      _tempC      = '--';
      _tempF      = '--';
      _deviceName = '';
      _errorMsg   = '';
    });
    }
  }

  // ─── Notification Handling ──────────────────────────────────────────────────

  void _handleIncoming(List<int> bytes) {
    _buffer += String.fromCharCodes(bytes);
    int idx;
    while ((idx = _buffer.indexOf('\r\n')) != -1) {
      final line = _buffer.substring(0, idx);
      _buffer    = _buffer.substring(idx + 2);
      _handleLine(line);
    }
  }

  void _handleLine(String line) {
    if (line.startsWith('TEMP:')) {
      try {
        final parts = line.substring(5).split(',');           // ["25.56C", "78.01F"]
        final c     = parts[0].substring(0, parts[0].length - 1);
        final f     = parts[1].substring(0, parts[1].length - 1);
        if (mounted) setState(() { _tempC = c; _tempF = f; _errorMsg = ''; _appState = AppState.streaming; });
      } catch (_) {
        if (mounted) setState(() => _errorMsg = 'Malformed TEMP packet: "$line"');
      }
    } else if (line.startsWith('ERROR:')) {
      final code = line.substring(6);
      if (mounted) setState(() => _errorMsg = _friendlyError(code));
    }
  }

  String _friendlyError(String code) => switch (code) {
    'SENSOR_FAIL'  => 'ZedBoard: I2C sensor read failed. Check hardware.',
    'UNKNOWN_CMD'  => 'ZedBoard received an unknown command.',
    _              => 'ZedBoard error: $code',
  };

  // ─── UI ─────────────────────────────────────────────────────────────────────

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D1117),
      appBar: AppBar(
        backgroundColor: const Color(0xFF0D1117),
        elevation: 0,
        title: const Text('BLE Temp Monitor',
            style: TextStyle(color: Colors.white, fontWeight: FontWeight.w600, letterSpacing: 0.5)),
        actions: [
          if (_appState == AppState.streaming || _appState == AppState.connected)
            TextButton(
              onPressed: _disconnect,
              child: const Text('Disconnect', style: TextStyle(color: Color(0xFF00BCD4))),
            ),
        ],
      ),
      body: SafeArea(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
          child: _buildBody(),
        ),
      ),
    );
  }

  Widget _buildBody() => switch (_appState) {
    AppState.disconnected => _buildDisconnected(),
    AppState.scanning     => _buildScanning(),
    AppState.connecting   => _buildConnecting(),
    AppState.connected    => _buildConnected(),
    AppState.streaming    => _buildStreaming(),
    AppState.error        => _buildError(),
  };

  Widget _buildDisconnected() => Center(
    child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      const Icon(Icons.bluetooth_disabled, size: 72, color: Color(0xFF4A5568)),
      const SizedBox(height: 24),
      const Text('Not connected', style: TextStyle(color: Colors.white70, fontSize: 18)),
      const SizedBox(height: 8),
      const Text('Tap Connect to scan for your ZedBoard.',
          style: TextStyle(color: Colors.white38, fontSize: 14), textAlign: TextAlign.center),
      const SizedBox(height: 40),
      _primaryButton('Connect', _startScan),
    ]),
  );

  Widget _buildScanning() => const Center(
    child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      CircularProgressIndicator(color: Color(0xFF00BCD4)),
      SizedBox(height: 24),
      Text('Scanning for ZedBoard…', style: TextStyle(color: Colors.white70, fontSize: 16)),
      SizedBox(height: 8),
      Text('Make sure the ZedBoard is powered and advertising.',
          style: TextStyle(color: Colors.white38, fontSize: 13), textAlign: TextAlign.center),
    ]),
  );

  Widget _buildConnecting() => Center(
    child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      const CircularProgressIndicator(color: Color(0xFF00BCD4)),
      const SizedBox(height: 24),
      Text('Connecting to $_deviceName…',
          style: const TextStyle(color: Colors.white70, fontSize: 16), textAlign: TextAlign.center),
    ]),
  );

  Widget _buildConnected() => Center(
    child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      const Icon(Icons.bluetooth_connected, size: 48, color: Color(0xFF00BCD4)),
      const SizedBox(height: 16),
      Text(_deviceName,
          style: const TextStyle(color: Colors.white, fontSize: 16, fontWeight: FontWeight.w600)),
      const SizedBox(height: 8),
      const Text('Connected — starting temperature stream…',
          style: TextStyle(color: Colors.white54, fontSize: 14)),
      const SizedBox(height: 24),
      const CircularProgressIndicator(color: Color(0xFF00BCD4), strokeWidth: 2),
    ]),
  );

  Widget _buildStreaming() => Column(
    crossAxisAlignment: CrossAxisAlignment.stretch,
    children: [
      Row(children: [
        const Icon(Icons.bluetooth_connected, size: 14, color: Color(0xFF00BCD4)),
        const SizedBox(width: 6),
        Text(_deviceName, style: const TextStyle(color: Color(0xFF00BCD4), fontSize: 13)),
        const Spacer(),
        Container(
          padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
          decoration: BoxDecoration(
            color: const Color(0xFF1A3A2A),
            borderRadius: BorderRadius.circular(12),
          ),
          child: const Row(children: [
            Icon(Icons.circle, size: 7, color: Color(0xFF4CAF50)),
            SizedBox(width: 4),
            Text('Live', style: TextStyle(color: Color(0xFF4CAF50), fontSize: 12)),
          ]),
        ),
      ]),
      const Spacer(),
      Center(
        child: Column(children: [
          Text('$_tempC°C',
              style: const TextStyle(
                  color: Colors.white, fontSize: 80, fontWeight: FontWeight.w200,
                  letterSpacing: -3, height: 1.0)),
          const SizedBox(height: 12),
          Text('$_tempF°F',
              style: const TextStyle(color: Colors.white54, fontSize: 40, fontWeight: FontWeight.w300)),
        ]),
      ),
      const SizedBox(height: 20),
      if (_errorMsg.isNotEmpty) _inlineError(_errorMsg),
      const Spacer(),
      const Center(
        child: Text('Updates every second', style: TextStyle(color: Colors.white24, fontSize: 12)),
      ),
      const SizedBox(height: 16),
    ],
  );

  Widget _buildError() => Center(
    child: Column(mainAxisAlignment: MainAxisAlignment.center, children: [
      const Icon(Icons.error_outline, size: 64, color: Color(0xFFEF5350)),
      const SizedBox(height: 24),
      Text(_errorMsg,
          style: const TextStyle(color: Colors.white70, fontSize: 15), textAlign: TextAlign.center),
      const SizedBox(height: 40),
      _primaryButton('Retry', _startScan),
      const SizedBox(height: 12),
      TextButton(
        onPressed: () => setState(() { _appState = AppState.disconnected; _errorMsg = ''; }),
        child: const Text('Cancel', style: TextStyle(color: Colors.white38)),
      ),
    ]),
  );

  Widget _primaryButton(String label, VoidCallback onPressed) => SizedBox(
    width: 200, height: 48,
    child: ElevatedButton(
      onPressed: onPressed,
      style: ElevatedButton.styleFrom(
        backgroundColor: const Color(0xFF00BCD4),
        foregroundColor: Colors.black,
        shape: RoundedRectangleBorder(borderRadius: BorderRadius.circular(8)),
      ),
      child: Text(label, style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 16)),
    ),
  );

  Widget _inlineError(String message) => Container(
    margin: const EdgeInsets.symmetric(vertical: 8),
    padding: const EdgeInsets.all(12),
    decoration: BoxDecoration(
      color: const Color(0xFF2D1515),
      borderRadius: BorderRadius.circular(8),
      border: Border.all(color: const Color(0xFFEF5350).withValues(alpha: 0.4)),
    ),
    child: Row(children: [
      const Icon(Icons.warning_amber_rounded, color: Color(0xFFEF5350), size: 18),
      const SizedBox(width: 10),
      Expanded(
        child: Text(message, style: const TextStyle(color: Color(0xFFEF9090), fontSize: 13)),
      ),
    ]),
  );
}