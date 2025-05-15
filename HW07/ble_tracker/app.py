# from flask import Flask, render_template, jsonify, send_from_directory, request
# from flask_sse import sse
# import math
# import asyncio
# import threading
# from bleak import BleakClient, BleakGATTCharacteristic
# import struct
# import time

# app = Flask(__name__)
# app.config["REDIS_URL"] = "redis://localhost"
# app.register_blueprint(sse, url_prefix='/stream')

# # BLE Configuration
# SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
# CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
# LED_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
# LED_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

# # Hardware configuration
# THRESHOLD = 1.0  # Distance threshold in meters
# ESP32_MAC_ADDRESSES = ["5c:01:3b:33:04:0a"]  # Verify MAC address
# devices = {}      # Real-time position data
# calibration_data = {}

# async def handle_notification(sender: BleakGATTCharacteristic, data: bytearray):
#     """Enhanced data processing handler"""
#     try:
#         decoded = data.decode('utf-8').strip()
#         if ',' not in decoded:
#             return
            
#         rssi_str, tx_power_str = decoded.split(',', 1)
#         client_id = f"client{sender.handle}"
#         rssi = int(rssi_str)
#         tx_power = int(tx_power_str)

#         # Distance calculation (assuming n=2.0)
#         distance = 10 ** ((tx_power - rssi) / 20)
#         angle = hash(client_id) % 360
        
#         devices[client_id] = {
#             'x': distance * math.cos(math.radians(angle)),
#             'y': distance * math.sin(math.radians(angle)),
#             'distance': distance
#         }

#         # SSE update
#         sse.publish({
#             "client_id": client_id,
#             "x": round(devices[client_id]['x'], 2),
#             "y": round(devices[client_id]['y'], 2),
#             "distance": round(distance, 2)
#         }, type='position')

#     except Exception as e:
#         print(f"Data processing error: {str(e)}")

# class BLEManager:
#     def __init__(self):
#         self.connected_devices = {}
        
#     async def connect_device(self, address):
#         """Enhanced connection handling logic"""
#         print(f"Attempting to connect to {address}...")
#         client = BleakClient(address, timeout=20.0)
#         try:
#             if not await client.connect():
#                 raise Exception("Connection failed")
            
#             # Wait for service discovery
#             await asyncio.sleep(2.0)
            
#             if not client.is_connected:
#                 raise Exception("Connection lost after initialization")
            
#             service = client.services.get_service(SERVICE_UUID)
#             if not service:
#                 raise Exception(f"Service {SERVICE_UUID} not found")
                
#             char = service.get_characteristic(CHARACTERISTIC_UUID)
#             if not char:
#                 raise Exception(f"Characteristic {CHARACTERISTIC_UUID} not found")
            
#             await client.start_notify(char.uuid, handle_notification)
#             self.connected_devices[address] = client
#             print(f"✅ {address} connected successfully")
#             return True
            
#         except Exception as e:
#             print(f"🔴 {address} connection failed: {str(e)}")
#             await client.disconnect()
#             return False

# async def run_ble_clients():
#     """Enhanced BLE client runner"""
#     manager = BLEManager()
#     while True:
#         tasks = [manager.connect_device(addr) for addr in ESP32_MAC_ADDRESSES]
#         await asyncio.gather(*tasks)
#         await asyncio.sleep(5.0)  # Reconnection interval

# def start_ble_clients():
#     loop = asyncio.new_event_loop()
#     asyncio.set_event_loop(loop)
#     loop.run_until_complete(run_ble_clients())

# async def send_led_command(r, g, b):
#     """Enhanced LED control function"""
#     try:
#         async with BleakClient(ESP32_MAC_ADDRESSES[0], timeout=10.0) as client:
#             await asyncio.sleep(0.5)  # Wait for service discovery
#             service = client.services.get_service(LED_SERVICE_UUID)
#             if not service:
#                 raise Exception("LED service not found")
                
#             char = service.get_characteristic(LED_CHAR_UUID)
#             if not char:
#                 raise Exception("LED characteristic not found")
            
#             data = f"{r},{g},{b}"
#             await client.write_gatt_char(char.uuid, data.encode())
#             print(f"LED set successfully: {data}")
#     except Exception as e:
#         print(f"LED control failed: {str(e)}")

# def control_led():
#     """Enhanced LED control logic"""
#     while True:
#         try:
#             led = {'r':0, 'g':0, 'b':0}
#             active_pairs = []
#             pairs = [
#                 ('client1', 'client2', 'r'),
#                 ('client2', 'client3', 'g'),
#                 ('client1', 'client3', 'b')
#             ]
            
#             for c1, c2, color in pairs:
#                 if c1 in devices and c2 in devices:
#                     distance = math.sqrt(
#                         (devices[c1]['x']-devices[c2]['x'])**2 +
#                         (devices[c1]['y']-devices[c2]['y'])**2
#                     )
#                     if distance < THRESHOLD:
#                         led[color] = 255
#                         active_pairs.append((c1, c2))

#             if len(active_pairs) == 3:
#                 led = {'r':255, 'g':255, 'b':255}

#             loop = asyncio.new_event_loop()
#             loop.run_until_complete(send_led_command(led['r'], led['g'], led['b']))
            
#         except Exception as e:
#             print(f"LED control error: {str(e)}")
        
#         time.sleep(1)

# @app.route('/')
# def index():
#     return render_template('index.html')

# @app.route('/calibrate', methods=['POST'])
# def calibrate():
#     try:
#         data = request.json
#         client_id = data['client_id']
#         actual_dist = float(data['actual_dist'])
        
#         if actual_dist <= 0:
#             return jsonify({"error": "Value must be greater than 0"}), 400
            
#         measured_dist = devices[client_id]['distance']
#         error = ((measured_dist - actual_dist) / actual_dist) * 100
        
#         calibration_data[client_id] = {
#             'actual': actual_dist,
#             'measured': measured_dist,
#             'error': round(error, 2)
#         }
#         return jsonify({"status": "success"})
        
#     except (KeyError, ValueError) as e:
#         return jsonify({"error": str(e)}), 400

# @app.route('/get_calibration')
# def get_calibration():
#     return jsonify(calibration_data)

# @app.route('/static/js/<path:filename>')
# def serve_js(filename):
#     return send_from_directory('static/js', filename)

# if __name__ == '__main__':
#     ble_thread = threading.Thread(target=start_ble_clients, daemon=True)
#     ble_thread.start()
    
#     led_thread = threading.Thread(target=control_led, daemon=True)
#     led_thread.start()
    
#     app.run(host='0.0.0.0', port=5000, use_reloader=False)


from flask import Flask, render_template, jsonify, send_from_directory, request
from flask_sse import sse
import math
import asyncio
import threading
import time
import logging
from bleak import BleakClient, BleakGATTCharacteristic, BleakScanner
import struct

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

app = Flask(__name__)
app.config["REDIS_URL"] = "redis://localhost"
app.register_blueprint(sse, url_prefix='/stream')

# BLE Configuration
SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
CHARACTERISTIC_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"
LED_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
LED_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

# Hardware configuration
THRESHOLD = 1.0  # Distance threshold in meters
ESP32_MAC_ADDRESSES = ["5c:01:3b:33:04:0a"]  # ESP32 서버 MAC 주소
ESP32_CLIENT_ADDRESSES = []  # 연결된 ESP32 클라이언트 MAC 주소를 저장할 리스트

devices = {}      # Real-time position data
calibration_data = {}
connected_clients = {}  # 연결된 클라이언트 정보

async def handle_notification(sender: BleakGATTCharacteristic, data: bytearray):
    """Enhanced data processing handler"""
    try:
        decoded = data.decode('utf-8').strip()
        logger.info(f"Received notification: {decoded}")
        
        if ',' not in decoded:
            logger.warning(f"Invalid data format: {decoded}")
            return
            
        rssi_str, tx_power_str = decoded.split(',', 1)
        client_id = f"client{sender.handle}"
        rssi = int(rssi_str)
        tx_power = int(tx_power_str)

        logger.info(f"Processed data - Client: {client_id}, RSSI: {rssi}, TX Power: {tx_power}")

        # Distance calculation (assuming n=2.0)
        distance = 10 ** ((tx_power - rssi) / 20)
        angle = hash(client_id) % 360
        
        devices[client_id] = {
            'x': distance * math.cos(math.radians(angle)),
            'y': distance * math.sin(math.radians(angle)),
            'distance': distance,
            'rssi': rssi,
            'tx_power': tx_power
        }

        # SSE update
        sse.publish({
            "client_id": client_id,
            "x": round(devices[client_id]['x'], 2),
            "y": round(devices[client_id]['y'], 2),
            "distance": round(distance, 2),
            "rssi": rssi,
            "tx_power": tx_power
        }, type='position')

    except Exception as e:
        logger.error(f"Data processing error: {str(e)}")

class BLEManager:
    def __init__(self):
        self.connected_devices = {}
        
    async def connect_device(self, address):
        """Enhanced connection handling logic"""
        logger.info(f"Attempting to connect to {address}...")
        
        # 이미 연결된 장치인지 확인
        if address in self.connected_devices and self.connected_devices[address].is_connected:
            logger.info(f"Device {address} already connected")
            return True
            
        client = BleakClient(address, timeout=20.0)
        try:
            await client.connect()
            logger.info(f"Connected to {address}")
            
            # 서비스 검색 대기
            await asyncio.sleep(2.0)
            
            # 연결 확인
            if not client.is_connected:
                raise Exception("Connection lost after initialization")
            
            # 서비스 및 특성 검색
            services = client.services
            service = None
            for svc in services:
                if svc.uuid.lower() == SERVICE_UUID.lower():
                    service = svc
                    break
                    
            if not service:
                raise Exception(f"Service {SERVICE_UUID} not found")
                
            # 특성 검색
            char = None
            for c in service.characteristics:
                if c.uuid.lower() == CHARACTERISTIC_UUID.lower():
                    char = c
                    break
                    
            if not char:
                raise Exception(f"Characteristic {CHARACTERISTIC_UUID} not found")
            
            # 알림 시작
            await client.start_notify(char.uuid, handle_notification)
            
            # 연결된 장치 목록에 추가
            self.connected_devices[address] = client
            connected_clients[address] = {
                'client': client,
                'service': service,
                'characteristic': char,
                'last_active': time.time()
            }
            
            logger.info(f"✅ {address} connected successfully")
            return True
            
        except Exception as e:
            logger.error(f"🔴 {address} connection failed: {str(e)}")
            try:
                await client.disconnect()
            except:
                pass
            return False

    async def discover_clients(self):
        """ESP32 클라이언트 장치 검색"""
        logger.info("Scanning for ESP32 client devices...")
        devices = await BleakScanner.discover()
        for device in devices:
            if device.name and "BLE-Client" in device.name:
                logger.info(f"Found ESP32 client: {device.address} ({device.name})")
                if device.address not in ESP32_CLIENT_ADDRESSES:
                    ESP32_CLIENT_ADDRESSES.append(device.address)

async def run_ble_clients():
    """Enhanced BLE client runner"""
    manager = BLEManager()
    
    while True:
        # 클라이언트 장치 검색
        await manager.discover_clients()
        
        # 서버 연결
        server_tasks = [manager.connect_device(addr) for addr in ESP32_MAC_ADDRESSES]
        await asyncio.gather(*server_tasks)
        
        # 클라이언트 연결
        client_tasks = [manager.connect_device(addr) for addr in ESP32_CLIENT_ADDRESSES]
        if client_tasks:
            await asyncio.gather(*client_tasks)
        
        # 5초 대기 후 다시 시도
        await asyncio.sleep(5.0)

async def send_led_command(r, g, b):
    """Enhanced LED control function"""
    if not ESP32_MAC_ADDRESSES:
        logger.error("No ESP32 server addresses configured")
        return False
        
    try:
        address = ESP32_MAC_ADDRESSES[0]
        logger.info(f"Sending LED command to {address}: R={r}, G={g}, B={b}")
        
        # 이미 연결된 장치인지 확인
        if address in connected_clients and connected_clients[address]['client'].is_connected:
            client = connected_clients[address]['client']
            char = connected_clients[address]['characteristic']
        else:
            # 새로 연결
            client = BleakClient(address, timeout=10.0)
            await client.connect()
            await asyncio.sleep(0.5)  # 서비스 검색 대기
            
            # 서비스 및 특성 검색
            services = client.services
            service = None
            for svc in services:
                if svc.uuid.lower() == LED_SERVICE_UUID.lower():
                    service = svc
                    break
                    
            if not service:
                raise Exception(f"LED service {LED_SERVICE_UUID} not found")
                
            char = None
            for c in service.characteristics:
                if c.uuid.lower() == LED_CHAR_UUID.lower():
                    char = c
                    break
                    
            if not char:
                raise Exception(f"LED characteristic {LED_CHAR_UUID} not found")
        
        # LED 명령 전송
        data = f"{r},{g},{b}"
        await client.write_gatt_char(char.uuid, data.encode())
        logger.info(f"LED set successfully: {data}")
        
        # 새로 연결한 경우 연결 해제
        if address not in connected_clients:
            await client.disconnect()
            
        return True
        
    except Exception as e:
        logger.error(f"LED control failed: {str(e)}")
        return False

def control_led():
    """Enhanced LED control logic"""
    while True:
        try:
            led = {'r':0, 'g':0, 'b':0}
            active_pairs = []
            
            # 장치가 2개 이상 있는지 확인
            if len(devices) >= 2:
                # 모든 장치 쌍 조합
                device_ids = list(devices.keys())
                pairs = []
                
                # 동적으로 쌍 생성
                if len(device_ids) >= 2:
                    pairs.append((device_ids[0], device_ids[1], 'r'))
                if len(device_ids) >= 3:
                    pairs.append((device_ids[1], device_ids[2], 'g'))
                    pairs.append((device_ids[0], device_ids[2], 'b'))
                
                for c1, c2, color in pairs:
                    if c1 in devices and c2 in devices:
                        distance = math.sqrt(
                            (devices[c1]['x']-devices[c2]['x'])**2 +
                            (devices[c1]['y']-devices[c2]['y'])**2
                        )
                        logger.info(f"Distance between {c1} and {c2}: {distance:.2f}m")
                        if distance < THRESHOLD:
                            led[color] = 255
                            active_pairs.append((c1, c2))

                if len(active_pairs) == 3:
                    led = {'r':255, 'g':255, 'b':255}
                    
                # LED 명령 전송
                loop = asyncio.new_event_loop()
                loop.run_until_complete(send_led_command(led['r'], led['g'], led['b']))
                loop.close()
            else:
                logger.info(f"Not enough devices connected: {len(devices)}")
                
        except Exception as e:
            logger.error(f"LED control error: {str(e)}")
        
        time.sleep(1)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    """시스템 상태 정보 제공"""
    return jsonify({
        "connected_servers": [addr for addr in ESP32_MAC_ADDRESSES if addr in connected_clients],
        "connected_clients": ESP32_CLIENT_ADDRESSES,
        "active_devices": list(devices.keys()),
        "device_data": devices,
        "calibration_data": calibration_data
    })

@app.route('/calibrate', methods=['POST'])
def calibrate():
    try:
        data = request.json
        client_id = data['client_id']
        actual_dist = float(data['actual_dist'])
        
        if actual_dist <= 0:
            return jsonify({"error": "Value must be greater than 0"}), 400
            
        if client_id not in devices:
            return jsonify({"error": f"Client {client_id} not found"}), 404
            
        measured_dist = devices[client_id]['distance']
        error = ((measured_dist - actual_dist) / actual_dist) * 100
        
        calibration_data[client_id] = {
            'actual': actual_dist,
            'measured': measured_dist,
            'error': round(error, 2)
        }
        return jsonify({"status": "success"})
        
    except (KeyError, ValueError) as e:
        return jsonify({"error": str(e)}), 400

@app.route('/get_calibration')
def get_calibration():
    return jsonify(calibration_data)

@app.route('/static/js/<path:filename>')
def serve_js(filename):
    return send_from_directory('static/js', filename)

if __name__ == '__main__':
    # BLE 클라이언트 스레드 시작
    ble_thread = threading.Thread(target=lambda: asyncio.run(run_ble_clients()), daemon=True)
    ble_thread.start()
    
    # LED 제어 스레드 시작
    led_thread = threading.Thread(target=control_led, daemon=True)
    led_thread.start()
    
    # 플라스크 서버 시작
    app.run(host='0.0.0.0', port=5000, use_reloader=False)
