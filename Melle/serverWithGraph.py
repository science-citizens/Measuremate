from dash.dependencies import Input, Output
from flask import Flask, request, jsonify
import plotly.graph_objs as go
from collections import deque
from dash import dcc, html
import threading
import dash
import time

# Flask app
app = Flask(__name__)

# Dash app for live visualization
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

# Constants
SENSOR_POSITIONS = [0, 17, 26, 43]  # Posities in cm
MAX_HISTORY = 5
MAX_VALUES = 20
NEAR_THRESHOLD = 38
EQUAL_MARGIN = 3
MOVEMENT_THRESHOLD = 4

# Globale opslag
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

sensor_history = deque(maxlen=MAX_HISTORY)

def classify_readings(sensor_values, near_threshold=NEAR_THRESHOLD, equal_margin=EQUAL_MARGIN):
    if len(sensor_values) != 4:
        return "Ongeldige invoer", []

    near_indices = [i for i, v in enumerate(sensor_values) if v < near_threshold]
    value_range = max(sensor_values) - min(sensor_values)

    def geschatte_breedte(indices):
        if len(indices) < 2:
            return 0
        left = SENSOR_POSITIONS[min(indices)]
        right = SENSOR_POSITIONS[max(indices)]
        return right - left

    if value_range <= equal_margin:
        if all(v < near_threshold for v in sensor_values):
            return "Groot object dichtbij (volledige dekking)", near_indices
        else:
            return "Groot/breed object op afstand", near_indices

    if not near_indices:
        return "Geen object zichtbaar", []

    pos_names = ["links", "midden-links", "midden-rechts", "rechts"]
    if len(near_indices) == 1:
        return f"Object dichtbij bij sensor {near_indices[0] + 1} ({pos_names[near_indices[0]]})", near_indices

    breedte = geschatte_breedte(near_indices)
    beschrijving = {
        2: "Twee sensoren – gedeeltelijk object",
        3: "Drie sensoren – waarschijnlijk breed object",
        4: "Alle sensoren – volledige dekking"
    }.get(len(near_indices), "Onbekende situatie")

    return f"{beschrijving} (geschatte breedte: {breedte} cm)", near_indices

def get_movement_direction(history, threshold=MOVEMENT_THRESHOLD):
    if len(history) < 2:
        return "Bewegingsrichting: onvoldoende gegevens"

    prev, latest = history[-2], history[-1]
    trends = []

    for i in range(4):
        delta = latest[i] - prev[i]
        if delta > threshold:
            trends.append("stijging")
        elif delta < -threshold:
            trends.append("daling")
        else:
            trends.append("gelijk")

    if trends == ["daling", "daling", "stijging", "stijging"]:
        return "Object beweegt van rechts naar links"
    elif trends == ["stijging", "stijging", "daling", "daling"]:
        return "Object beweegt van links naar rechts"
    elif all(t == "daling" for t in trends):
        return "Object komt dichterbij"
    elif all(t == "stijging" for t in trends):
        return "Object beweegt weg"
    elif all(t == "gelijk" for t in trends):
        return "Geen duidelijke beweging"

    return f"Beweging per sensor: {', '.join(trends)}"

# Dash layout
dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=500, n_intervals=0)
])

@dash_app.callback(
    [Output('live-graph', 'figure'),
     Output('classification-output', 'children')],
    [Input('interval-update', 'n_intervals')]
)
def update_graph(n):
    if not sensor_data["sensor_1"]:
        return go.Figure(), "Nog geen data ontvangen"

    latest_values = [
        sensor_data["sensor_1"][-1],
        sensor_data["sensor_2"][-1],
        sensor_data["sensor_3"][-1],
        sensor_data["sensor_4"][-1]
    ]

    sensor_history.append(latest_values)

    classificatie, _ = classify_readings(latest_values)
    richting = get_movement_direction(sensor_history)

    fig = go.Figure(
        data=[go.Scatter(
            x=["Sensor 1", "Sensor 2", "Sensor 3", "Sensor 4"],
            y=latest_values,
            mode='lines+markers',
            line=dict(color='royalblue', width=3),
            marker=dict(size=8)
        )],
        layout=go.Layout(
            title='Laatste Sonar Sensorwaarden',
            xaxis={'title': 'Sensoren'},
            yaxis={'title': 'Afstand (cm)', 'range': [0, max(latest_values) + 20]}
        )
    )

    return fig, f"{classificatie} | {richting}"

@app.route('/data', methods=['POST'])
def receive_data():
    if not request.is_json:
        return jsonify({"status": "error", "message": "Inhoud is geen JSON"}), 400

    try:
        data = request.get_json()
        timestamp = time.strftime("%H:%M:%S")
        current_readings = []

        for i in range(1, 5):
            key = f"sensor_{i}"
            value = data.get(key)
            if value is None or not isinstance(value, (int, float)):
                return jsonify({"status": "error", "message": f"Foutieve waarde voor {key}"}), 400

            sensor_data[key].append(value)
            current_readings.append(value)
            if len(sensor_data[key]) > MAX_VALUES:
                sensor_data[key].pop(0)

        sensor_data["timestamps"].append(timestamp)
        if len(sensor_data["timestamps"]) > MAX_VALUES:
            sensor_data["timestamps"].pop(0)

        print(f"[{timestamp}] Received: {data}")
        return jsonify({"status": "success", "received": data})

    except Exception as e:
        print(f"Fout bij verwerken van data: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

# Flask in aparte thread
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# Main
if __name__ == '__main__':
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()
    dash_app.run_server(debug=True, port=8050, use_reloader=False)
