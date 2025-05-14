from flask import Flask, render_template, request, jsonify, send_from_directory
from flask_sse import sse
import math
import requests
import threading

app = Flask(__name__)
app.config["REDIS_URL"] = "redis://localhost"
app.register_blueprint(sse, url_prefix='/stream')

# 하드웨어 설정
ESP32_SERVER_IP = "192.168.0.50"  # ESP32 서버 IP 주소
LED_ENDPOINT = f"http://{ESP32_SERVER_IP}/set_led"

# 전역 변수
devices = {}          # 실시간 위치 데이터
calibration_data = {} # 교정 데이터
THRESHOLD = 1.0       # 거리 임계값 (미터)

def calculate_distance(c1, c2):
    """두 클라이언트 간 유클리드 거리 계산"""
    dx = devices[c1]['x'] - devices[c2]['x']
    dy = devices[c1]['y'] - devices[c2]['y']
    return math.sqrt(dx**2 + dy**2)

def control_led():
    """1초 간격으로 거리 체크 및 LED 제어"""
    while True:
        led = {'r':0, 'g':0, 'b':0}
        active_pairs = []
        
        # 가능한 모든 클라이언트 조합
        pairs = [
            ('client1', 'client2'),
            ('client2', 'client3'), 
            ('client1', 'client3')
        ]
        
        # 각 조합 거리 측정
        for pair in pairs:
            c1, c2 = pair
            if c1 in devices and c2 in devices:
                distance = calculate_distance(c1, c2)
                if distance < THRESHOLD:
                    active_pairs.append(pair)
        
        # LED 컬러 결정
        if ('client1', 'client2') in active_pairs:
            led['r'] = 255
        if ('client2', 'client3') in active_pairs:
            led['g'] = 255
        if ('client1', 'client3') in active_pairs:
            led['b'] = 255
        if len(active_pairs) == 3:
            led = {'r':255, 'g':255, 'b':255}
        
        # ESP32에 LED 명령 전송
        try:
            requests.post(LED_ENDPOINT, json=led, timeout=1)
        except Exception as e:
            print("LED control failed:", e)
        
        threading.Event().wait(1)

# 백그라운드 스레드 시작
threading.Thread(target=control_led, daemon=True).start()

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/update', methods=['POST'])
def update_data():
    data = request.json
    client_id = data['client_id']
    rssi = data['rssi']
    tx_power = data['tx_power']
    
    # 거리 계산 (n=2.0 가정)
    distance = 10 ** ((tx_power - rssi) / 20)
    angle = hash(client_id) % 360  # 장치별 고유 각도
    
    # 좌표 저장
    devices[client_id] = {
        'x': distance * math.cos(math.radians(angle)),
        'y': distance * math.sin(math.radians(angle)),
        'distance': distance
    }
    
    # 실시간 업데이트 전송
    sse.publish({
        "client_id": client_id,
        "x": devices[client_id]['x'],
        "y": devices[client_id]['y'],
        "distance": distance
    }, type='position')
    
    return jsonify({"status": "success"})

@app.route('/calibrate', methods=['POST'])
def calibrate():
    data = request.json
    try:
        client_id = data['client_id']
        actual_dist = float(data['actual_dist'])
        
        if actual_dist <= 0:
            return jsonify({"error": "Distance must be positive"}), 400
            
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
    app.run(host='0.0.0.0', port=5000)
