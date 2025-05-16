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
from bleak import BleakClient, BleakGATTCharacteristic
import struct
import time

app = Flask(__name__)
app.config["REDIS_URL"] = "redis://localhost"
app.register_blueprint(sse, url_prefix='/stream')

# BLE Configuration
SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  # UART 서비스 UUID
CHARACTERISTIC_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  # RX 특성 UUID
LED_SERVICE_UUID = "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
LED_CHAR_UUID = "beb5483e-36e1-4688-b7f5-ea07361b26a8"

# Hardware configuration
THRESHOLD = 1.0  # Distance threshold in meters
ESP32_MAC_ADDRESSES = ["5c:01:3b:33:04:0a"]  # Verify MAC address
devices = {}      # Real-time position data
calibration_data = {}

async def handle_notification(sender: BleakGATTCharacteristic, data: bytearray):
    """ESP32에서 직접 계산된 거리 데이터를 처리하는 핸들러"""
    try:
        decoded = data.decode('utf-8').strip()
        client_id = f"client{sender.handle}"
        
        # ESP32에서 직접 계산된 거리 값 파싱
        distance = float(decoded)
        
        # 간단한 위치 계산 (각도는 클라이언트 ID에 따라 임의로 설정)
        angle = hash(client_id) % 360
        
        devices[client_id] = {
            'x': distance * math.cos(math.radians(angle)),
            'y': distance * math.sin(math.radians(angle)),
            'distance': distance
        }

        print(f"수신된 거리: {distance}m (클라이언트 ID: {client_id})")

        # SSE를 통해 웹 인터페이스에 업데이트
        sse.publish({
            "client_id": client_id,
            "x": round(devices[client_id]['x'], 2),
            "y": round(devices[client_id]['y'], 2),
            "distance": round(distance, 2)
        }, type='position')

    except Exception as e:
        print(f"데이터 처리 오류: {str(e)}")

class BLEManager:
    def __init__(self):
        self.connected_devices = {}
        
    async def connect_device(self, address):
        """BLE 장치 연결 및 알림 설정"""
        print(f"{address}에 연결 시도 중...")
        client = BleakClient(address, timeout=20.0)
        try:
            if not await client.connect():
                raise Exception("연결 실패")
            
            # 서비스 검색을 위한 대기
            await asyncio.sleep(2.0)
            
            if not client.is_connected:
                raise Exception("초기화 후 연결 끊김")
            
            # 서비스 및 특성 찾기
            services = client.services
            for service in services:
                if service.uuid.lower() == SERVICE_UUID.lower():
                    for char in service.characteristics:
                        if char.uuid.lower() == CHARACTERISTIC_UUID.lower():
                            # 알림 활성화
                            await client.start_notify(char.uuid, handle_notification)
                            self.connected_devices[address] = client
                            print(f"✅ {address} 연결 성공 및 알림 설정 완료")
                            return True
            
            raise Exception(f"서비스 또는 특성을 찾을 수 없음")
            
        except Exception as e:
            print(f"🔴 {address} 연결 실패: {str(e)}")
            if client.is_connected:
                await client.disconnect()
            return False

async def run_ble_clients():
    """BLE 클라이언트 실행 및 재연결 관리"""
    manager = BLEManager()
    while True:
        for addr in ESP32_MAC_ADDRESSES:
            if addr not in manager.connected_devices or not manager.connected_devices[addr].is_connected:
                await manager.connect_device(addr)
        await asyncio.sleep(5.0)  # 재연결 간격

def start_ble_clients():
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(run_ble_clients())

async def send_led_command(r, g, b):
    """LED 제어 함수"""
    try:
        async with BleakClient(ESP32_MAC_ADDRESSES[0], timeout=10.0) as client:
            await asyncio.sleep(0.5)  # 서비스 검색 대기
            
            # 서비스 및 특성 찾기
            services = client.services
            for service in services:
                if service.uuid.lower() == LED_SERVICE_UUID.lower():
                    for char in service.characteristics:
                        if char.uuid.lower() == LED_CHAR_UUID.lower():
                            data = f"{r},{g},{b}"
                            await client.write_gatt_char(char.uuid, data.encode())
                            print(f"LED 설정 성공: {data}")
                            return
            
            raise Exception("LED 서비스 또는 특성을 찾을 수 없음")
    except Exception as e:
        print(f"LED 제어 실패: {str(e)}")

def control_led():
    """거리에 따른 LED 제어 로직"""
    while True:
        try:
            led = {'r':0, 'g':0, 'b':0}
            active_pairs = []
            pairs = [
                ('client1', 'client2', 'r'),
                ('client2', 'client3', 'g'),
                ('client1', 'client3', 'b')
            ]
            
            for c1, c2, color in pairs:
                if c1 in devices and c2 in devices:
                    distance = math.sqrt(
                        (devices[c1]['x']-devices[c2]['x'])**2 +
                        (devices[c1]['y']-devices[c2]['y'])**2
                    )
                    if distance < THRESHOLD:
                        led[color] = 255
                        active_pairs.append((c1, c2))

            if len(active_pairs) == 3:
                led = {'r':255, 'g':255, 'b':255}

            loop = asyncio.new_event_loop()
            loop.run_until_complete(send_led_command(led['r'], led['g'], led['b']))
            
        except Exception as e:
            print(f"LED 제어 오류: {str(e)}")
        
        time.sleep(1)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/calibrate', methods=['POST'])
def calibrate():
    try:
        data = request.json
        client_id = data['client_id']
        actual_dist = float(data['actual_dist'])
        
        if actual_dist <= 0:
            return jsonify({"error": "값은 0보다 커야 합니다"}), 400
            
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
    ble_thread = threading.Thread(target=start_ble_clients, daemon=True)
    ble_thread.start()
    
    led_thread = threading.Thread(target=control_led, daemon=True)
    led_thread.start()
    
    app.run(host='0.0.0.0', port=5000, use_reloader=False)
