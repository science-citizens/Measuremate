from dash.dependencies import Input, Output
from flask import Flask, request, jsonify
import plotly.graph_objs as go
from collections import deque
from dash import dcc, html
import threading
import dash
import time

# === Flask API voor data-inname ===
app = Flask(__name__)

# === Dash-app voor realtime visualisatie ===
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

# === Constante instellingen ===
MAX_HISTORY = 5             # Aantal historische metingen om in geheugen te houden
MAX_VALUES = 20             # Maximaal aantal te plotten waarden per sensor
GRADIENT_THRESHOLD = 3      # Grens voor betekenisvolle verandering tussen sensorwaarden

# === Globale opslag voor binnenkomende metingen ===
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

# Tijdreeksenbuffer voor latere uitbreiding (bijv. richtingdetectie)
sensor_history = deque(maxlen=MAX_HISTORY)

# === Classificatie obv verschil (gradient) tussen sensoren ===
def classify_by_gradient(sensor_values, threshold=GRADIENT_THRESHOLD):
    if len(sensor_values) != 4:
        return "Ongeldige invoer"

    # Bereken verschillen tussen opeenvolgende sensoren
    gradients = [sensor_values[i+1] - sensor_values[i] for i in range(3)]

    # Interpretatie van gradient-patronen
    if all(abs(g) < threshold for g in gradients):
        return "Object recht voor het systeem"
    elif all(g > threshold for g in gradients):
        return "Object komt schuin van links (/)"
    elif all(g < -threshold for g in gradients):
        return "Object komt schuin van rechts (\\)"
    elif gradients[0] < -threshold and gradients[1] > threshold:
        return "Object schuin rechtsvoor"
    elif gradients[0] > threshold and gradients[1] < -threshold:
        return "Object schuin linksvoor"
    else:
        return "Onregelmatig object of meerdere reflecties"

# === Layout van de Dash-webinterface ===
dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),  # Linechart van sonarwaarden
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=500, n_intervals=0)  # Automatische update elke 500ms
])

# === Callback voor grafiekupdate en classificatie ===
@dash_app.callback(
    [Output('live-graph', 'figure'),
     Output('classification-output', 'children')],
    [Input('interval-update', 'n_intervals')]
)
def update_graph(n):
    # Wacht tot er metingen zijn
    if not sensor_data["sensor_1"]:
        return go.Figure(), "Nog geen data ontvangen"

    # Pak laatste meting van elke sensor
    latest_values = [
        sensor_data["sensor_1"][-1],
        sensor_data["sensor_2"][-1],
        sensor_data["sensor_3"][-1],
        sensor_data["sensor_4"][-1]
    ]

    # Voeg toe aan geschiedenisbuffer
    sensor_history.append(latest_values)

    # Voer classificatie uit
    classificatie = classify_by_gradient(latest_values)

    # Genereer de visualisatiegrafiek
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

    return fig, classificatie

# === API endpoint: ontvang nieuwe sensorwaarden ===
@app.route('/data', methods=['POST'])
def receive_data():
    if not request.is_json:
        return jsonify({"status": "error", "message": "Inhoud is geen JSON"}), 400

    try:
        data = request.get_json()
        timestamp = time.strftime("%H:%M:%S")
        current_readings = []

        # Verwerk waarden van de vier sensoren
        for i in range(1, 5):
            key = f"sensor_{i}"
            value = data.get(key)
            if value is None or not isinstance(value, (int, float)):
                return jsonify({"status": "error", "message": f"Foutieve waarde voor {key}"}), 400

            sensor_data[key].append(value)
            current_readings.append(value)

            # Beperk lengte van opgeslagen data
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

# === Start de Flask-server in aparte thread (voor gelijktijdige Dash-server) ===
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# === Applicatiestartpunt ===
if __name__ == '__main__':
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()

    # Start de Dash-frontend
    dash_app.run_server(debug=True, port=8050, use_reloader=False)