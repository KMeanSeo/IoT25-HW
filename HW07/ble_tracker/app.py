from flask import Flask, render_template, request, jsonify, send_from_directory
from flask_sse import sse
import math

app = Flask(__name__)
app.config["REDIS_URL"] = "redis://localhost"
app.register_blueprint(sse, url_prefix='/stream')

devices = {}
calibration_data = {}

def calculate_distance(rssi, tx_power, n=2.0):
    return round(10 ** ((tx_power - rssi) / (10 * n)), 2)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/update', methods=['POST'])
def update_data():
    data = request.json
    client_id = data['client_id']
    rssi = data['rssi']
    tx_power = data['tx_power']
    distance = calculate_distance(rssi, tx_power)
    devices[client_id] = distance
    angle = hash(client_id) % 360
    x = distance * math.cos(math.radians(angle))
    y = distance * math.sin(math.radians(angle))
    sse.publish({
        "client_id": client_id,
        "x": x,
        "y": y,
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
        measured_dist = devices.get(client_id, 0)
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
    app.run(host='0.0.0.0', port=5000, debug=True)
