from dash.dependencies import Input, Output
from flask import Flask, request, jsonify
import plotly.graph_objs as go
from collections import deque
from dash import dcc, html
import threading
import dash
import time

# === Flask API ===
app = Flask(__name__)  # Verantwoordelijk voor dataontvangst van Arduino

# === Dash dashboard ===
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')  # Realtime visualisatie

# === Constante instellingen ===
SENSOR_POSITIONS = [0, 17, 26, 43]  # Positie van sonar sensoren in cm
MAX_HISTORY = 5                    # Max aantal meetreeksen voor historie
MAX_VALUES = 20                   # Max aantal datapunten per sensor in opslag
NEAR_THRESHOLD = 40              # Afstand (cm) waaronder een object als "dichtbij" wordt gezien
EQUAL_MARGIN = 3                 # Tolerantie voor gelijke afstanden (voor classificatie)

# === Data opslag voor sensoren ===
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

# Historie buffer voor analyse van meerdere metingen (indien gewenst)
sensor_history = deque(maxlen=MAX_HISTORY)

# === Classificatie van meetwaarden ===
def classify_readings(sensor_values, near_threshold=NEAR_THRESHOLD, equal_margin=EQUAL_MARGIN):
    if len(sensor_values) != 4:
        return "Ongeldige invoer", []

    # Welke sensoren meten een object binnen de threshold?
    near_indices = [i for i, v in enumerate(sensor_values) if v < near_threshold]
    value_range = max(sensor_values) - min(sensor_values)

    # Hulpfunctie om breedte van object te schatten
    def geschatte_breedte(indices):
        if len(indices) < 2:
            return 0
        left = SENSOR_POSITIONS[min(indices)]
        right = SENSOR_POSITIONS[max(indices)]
        return right - left

    # Als alle waarden ongeveer gelijk zijn maar niet dichtbij => object op afstand
    if value_range <= equal_margin:
        if not all(v < near_threshold for v in sensor_values):
            return "Groot/breed object op afstand", near_indices

    # Als geen enkele sensor iets meet
    if not near_indices:
        return "Geen object zichtbaar", []

    # Positienamen voor extra duidelijkheid
    pos_names = ["links", "midden-links", "midden-rechts", "rechts"]

    if len(near_indices) == 1:
        return f"Object dichtbij bij sensor {near_indices[0] + 1} ({pos_names[near_indices[0]]})", near_indices

    breedte = geschatte_breedte(near_indices)
    beschrijving = {
        2: "Twee sensoren  gedeeltelijk object",
        3: "Drie sensoren  breed object",
        4: "Alle sensoren  volledige sensor dekking"
    }.get(len(near_indices), "Onbekende situatie")

    return f"{beschrijving} (minimale breedte: {breedte} cm)", near_indices

# === Dash layout voor live sensorweergave ===
dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=500, n_intervals=0)  # Update elke 0.5 sec
])

# === Callback: update grafiek en classificatie op basis van nieuwe data ===
@dash_app.callback(
    [Output('live-graph', 'figure'),
     Output('classification-output', 'children')],
    [Input('interval-update', 'n_intervals')]
)
def update_graph(n):
    if not sensor_data["sensor_1"]:  # Nog geen data ontvangen
        return go.Figure(), "Nog geen data ontvangen"

    # Pak laatste meting per sensor
    latest_values = [
        sensor_data["sensor_1"][-1],
        sensor_data["sensor_2"][-1],
        sensor_data["sensor_3"][-1],
        sensor_data["sensor_4"][-1]
    ]

    sensor_history.append(latest_values)

    classificatie, _ = classify_readings(latest_values)

    # Plot de waarden
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

# === API-endpoint voor het ontvangen van sensorwaarden via POST ===
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

            # Validatie van invoer
            if value is None or not isinstance(value, (int, float)):
                return jsonify({"status": "error", "message": f"Foutieve waarde voor {key}"}), 400

            sensor_data[key].append(value)
            current_readings.append(value)

            if len(sensor_data[key]) > MAX_VALUES:
                sensor_data[key].pop(0)

        # Voeg tijdstip toe
        sensor_data["timestamps"].append(timestamp)
        if len(sensor_data["timestamps"]) > MAX_VALUES:
            sensor_data["timestamps"].pop(0)

        print(f"[{timestamp}] Received: {data}")
        return jsonify({"status": "success", "received": data})

    except Exception as e:
        print(f"Fout bij verwerken van data: {e}")
        return jsonify({"status": "error", "message": str(e)}), 500

# === Start Flask server in aparte thread zodat Dash en Flask tegelijk kunnen draaien ===
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# === Startpunt van script ===
if __name__ == '__main__':
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()

    # Start de Dash app (visualisatie)
    dash_app.run_server(debug=True, port=8050, use_reloader=False)