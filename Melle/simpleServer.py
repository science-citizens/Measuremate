# Simple server for receiving data from the sonar sensors

from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route('/data', methods=['POST'])
def receive_data():
    try:
        raw_data = request.get_data(as_text=True)  # Print raw request data
        print(f"Raw Data Received: {raw_data}")

        return jsonify({"status": "success", "received": raw_data})

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)
