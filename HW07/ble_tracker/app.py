from flask import Flask, render_template, jsonify, send_from_directory, request
from flask_sse import sse
import math
import asyncio
import threading
from bleak import BleakClient, BleakGATTCharacteristic, BleakScanner
import struct
import time
import logging

# 로깅 설정
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('ble_client.log')
    ]
)
logger = logging.getLogger("BLE_Client")

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
ESP32_MAC_ADDRESSES = ["08:a6:f7:a1:46:fa"]  # ESP32 서버 MAC 주소
devices = {}      # Real-time position data
calibration_data = {}

# 연결 상태 관리
connection_status = {addr: False for addr in ESP32_MAC_ADDRESSES}
last_reconnect_attempt = {addr: 0 for addr in ESP32_MAC_ADDRESSES}
reconnect_interval = 5  # 재연결 시도 간격 (초)

async def handle_notification(sender: BleakGATTCharacteristic, data: bytearray):
    """ESP32에서 직접 계산된 거리 데이터를 처리하는 핸들러"""
    try:
        decoded = data.decode('utf-8').strip()
        client_id = f"client{sender.handle}"
        
        # ESP32에서 직접 계산된 거리 값 파싱
        try:
            distance = float(decoded)
        except ValueError:
            logger.warning(f"잘못된 거리 데이터 형식: {decoded}")
            return
        
        # 간단한 위치 계산 (각도는 클라이언트 ID에 따라 임의로 설정)
        angle = hash(client_id) % 360
        
        devices[client_id] = {
            'x': distance * math.cos(math.radians(angle)),
            'y': distance * math.sin(math.radians(angle)),
            'distance': distance
        }

        logger.info(f"수신된 거리: {distance}m (클라이언트 ID: {client_id})")

        # SSE를 통해 웹 인터페이스에 업데이트
        sse.publish({
            "client_id": client_id,
            "x": round(devices[client_id]['x'], 2),
            "y": round(devices[client_id]['y'], 2),
            "distance": round(distance, 2)
        }, type='position')

    except Exception as e:
        logger.error(f"데이터 처리 오류: {str(e)}")

class BLEManager:
    def __init__(self):
        self.connected_devices = {}
        self.connection_lock = asyncio.Lock()
        
    async def scan_for_device(self, address, timeout=5.0):
        """지정된 MAC 주소를 가진 장치 스캔"""
        logger.info(f"{address} 장치 스캔 중...")
        try:
            device = await BleakScanner.find_device_by_address(address, timeout=timeout)
            if device:
                logger.info(f"장치 발견: {device.name} ({device.address})")
                return device
            logger.warning(f"{address} 장치를 찾을 수 없음")
            return None
        except Exception as e:
            logger.error(f"스캔 오류: {str(e)}")
            return None
        
    async def connect_device(self, address):
        """BLE 장치 연결 및 알림 설정 (개선된 버전)"""
        # 이미 연결 시도 중인지 확인
        async with self.connection_lock:
            if address in self.connected_devices and self.connected_devices[address].is_connected:
                logger.info(f"{address}에 이미 연결되어 있음")
                return True
                
            # 재연결 간격 확인
            current_time = time.time()
            if current_time - last_reconnect_attempt[address] < reconnect_interval:
                return False
                
            last_reconnect_attempt[address] = current_time
            
        logger.info(f"{address}에 연결 시도 중...")
        
        # 먼저 장치 스캔
        device = await self.scan_for_device(address)
        if not device:
            logger.error(f"{address} 장치를 찾을 수 없어 연결 실패")
            return False
            
        # 연결 시도
        client = BleakClient(device, timeout=20.0)
        try:
            # 연결 시도
            await client.connect()
            if not client.is_connected:
                raise Exception("연결 실패")
            
            logger.info(f"{address}에 연결됨")
            
            # 서비스 검색을 위한 대기
            await asyncio.sleep(1.0)
            
            # 서비스 및 특성 찾기
            services = client.services
            uart_service = None
            uart_char = None
            
            for service in services:
                if service.uuid.lower() == SERVICE_UUID.lower():
                    uart_service = service
                    for char in service.characteristics:
                        if char.uuid.lower() == CHARACTERISTIC_UUID.lower():
                            uart_char = char
                            break
                    if uart_char:
                        break
            
            if not uart_service or not uart_char:
                raise Exception(f"UART 서비스 또는 특성을 찾을 수 없음")
                
            # 알림 활성화
            await client.start_notify(uart_char.uuid, handle_notification)
            
            # 연결 상태 업데이트
            async with self.connection_lock:
                self.connected_devices[address] = client
                connection_status[address] = True
                
            logger.info(f"✅ {address} 연결 성공 및 알림 설정 완료")
            return True
            
        except Exception as e:
            logger.error(f"🔴 {address} 연결 실패: {str(e)}")
            try:
                if client.is_connected:
                    await client.disconnect()
            except:
                pass
                
            connection_status[address] = False
            return False

    async def disconnect_device(self, address):
        """장치 연결 해제"""
        async with self.connection_lock:
            if address in self.connected_devices:
                client = self.connected_devices[address]
                if client.is_connected:
                    try:
                        await client.disconnect()
                        logger.info(f"{address} 연결 해제됨")
                    except Exception as e:
                        logger.error(f"{address} 연결 해제 오류: {str(e)}")
                del self.connected_devices[address]
                connection_status[address] = False

async def run_ble_clients():
    """BLE 클라이언트 실행 및 재연결 관리 (개선된 버전)"""
    manager = BLEManager()
    
    while True:
        for addr in ESP32_MAC_ADDRESSES:
            # 연결 상태 확인
            is_connected = False
            if addr in manager.connected_devices:
                client = manager.connected_devices[addr]
                try:
                    is_connected = client.is_connected
                except:
                    is_connected = False
                    
            # 연결되지 않은 경우 연결 시도
            if not is_connected:
                await manager.connect_device(addr)
            else:
                # 연결 상태 유지를 위한 하트비트 (선택 사항)
                logger.debug(f"{addr}에 연결 유지 중")
                
        # 적절한 대기 시간
        await asyncio.sleep(1.0)

def start_ble_clients():
    """BLE 클라이언트 스레드 시작"""
    logger.info("BLE 클라이언트 스레드 시작")
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(run_ble_clients())

async def send_led_command(r, g, b):
    """LED 제어 함수 (개선된 버전)"""
    for attempt in range(3):  # 최대 3번 시도
        try:
            # 장치 스캔
            scanner = BLEManager()
            device = await scanner.scan_for_device(ESP32_MAC_ADDRESSES[0])
            if not device:
                logger.error("LED 제어를 위한 장치를 찾을 수 없음")
                await asyncio.sleep(1)
                continue
                
            # 연결
            async with BleakClient(device, timeout=10.0) as client:
                if not client.is_connected:
                    logger.error("LED 제어를 위한 연결 실패")
                    await asyncio.sleep(1)
                    continue
                    
                # 서비스 및 특성 찾기
                led_service = None
                led_char = None
                
                for service in client.services:
                    if service.uuid.lower() == LED_SERVICE_UUID.lower():
                        led_service = service
                        for char in service.characteristics:
                            if char.uuid.lower() == LED_CHAR_UUID.lower():
                                led_char = char
                                break
                        if led_char:
                            break
                
                if not led_service or not led_char:
                    logger.error("LED 서비스 또는 특성을 찾을 수 없음")
                    return
                    
                # 데이터 전송
                data = f"{r},{g},{b}"
                await client.write_gatt_char(led_char.uuid, data.encode())
                logger.info(f"LED 설정 성공: {data}")
                return
                
        except Exception as e:
            logger.error(f"LED 제어 시도 {attempt+1} 실패: {str(e)}")
            await asyncio.sleep(1)
            
    logger.error("모든 LED 제어 시도 실패")

def control_led():
    """거리에 따른 LED 제어 로직"""
    logger.info("LED 제어 스레드 시작")
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
            logger.error(f"LED 제어 오류: {str(e)}")
        
        time.sleep(1)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/status')
def status():
    """BLE 연결 상태 확인 API"""
    return jsonify({
        "devices": {addr: connection_status[addr] for addr in ESP32_MAC_ADDRESSES},
        "active_clients": len(devices)
    })

@app.route('/calibrate', methods=['POST'])
def calibrate():
    try:
        data = request.json
        client_id = data['client_id']
        actual_dist = float(data['actual_dist'])
        
        if actual_dist <= 0:
            return jsonify({"error": "값은 0보다 커야 합니다"}), 400
            
        if client_id not in devices:
            return jsonify({"error": "해당 클라이언트 ID가 존재하지 않습니다"}), 404
            
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
    logger.info("BLE 거리 측정 애플리케이션 시작")
    
    # BLE 클라이언트 스레드 시작
    ble_thread = threading.Thread(target=start_ble_clients, daemon=True)
    ble_thread.start()
    
    # LED 제어 스레드 시작
    led_thread = threading.Thread(target=control_led, daemon=True)
    led_thread.start()
    
    # Flask 웹 서버 시작
    app.run(host='0.0.0.0', port=5000, use_reloader=False)
