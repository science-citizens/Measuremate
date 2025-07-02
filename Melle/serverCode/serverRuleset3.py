from dash.dependencies import Input, Output
from flask import Flask, request, jsonify
import plotly.graph_objs as go
from collections import deque
from dash import dcc, html
import threading
import dash
import time

# === Flask server voor het ontvangen van sensor data ===
app = Flask(__name__)

# === Dash webdashboard voor realtime visualisatie ===
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

# === Configuratieinstellingen ===
MAX_HISTORY = 5             # Niet in gebruik in deze versie, maar gereserveerd voor uitbreiding
MAX_VALUES = 20             # Aantal meetpunten dat per sensor wordt bijgehouden
MOVEMENT_THRESHOLD = 4      # Drempelwaarde om beweging als significant te beschouwen

# === Sensor data opslagstructuur ===
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

# === Tijdreeksenbuffer met de laatste 2 metingen (voor bewegingsanalyse) ===
sensor_history = deque(maxlen=2)

# === Functie voor het analyseren van beweging tussen twee meetmomenten ===
def analyseer_beweging(history, threshold=MOVEMENT_THRESHOLD):
    if len(history) < 2:
        return "Onvoldoende gegevens"

    vorige, laatste = history[0], history[1]

    trends = []
    for i in range(4):
        verschil = laatste[i] - vorige[i]
        if verschil > threshold:
            trends.append("verderaf")
        elif verschil < -threshold:
            trends.append("dichterbij")
        else:
            trends.append("gelijk")

    # Herken bekende patronen op basis van trends
    if trends == ["dichterbij"] * 4:
        return "Object komt dichterbij"
    elif trends == ["verderaf"] * 4:
        return "Object beweegt weg"
    elif trends == ["dichterbij", "dichterbij", "verderaf", "verderaf"]:
        return "Object beweegt van rechts naar links"
    elif trends == ["verderaf", "verderaf", "dichterbij", "dichterbij"]:
        return "Object beweegt van links naar rechts"
    else:
        return f"Beweging per sensor: {', '.join(trends)}"

# === Dash layout opbouw ===
dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=1000, n_intervals=0)  # Update elke seconde
])

# === Callback functie die de grafiek en bewegingsanalyse bijwerkt ===
@dash_app.callback(
    [Output('live-graph', 'figure'),
     Output('classification-output', 'children')],
    [Input('interval-update', 'n_intervals')]
)
def update_graph(n):
    if not sensor_data["sensor_1"]:
        return go.Figure(), "Nog geen data ontvangen"

    # Pak de nieuwste waarden per sensor
    latest_values = [
        sensor_data["sensor_1"][-1],
        sensor_data["sensor_2"][-1],
        sensor_data["sensor_3"][-1],
        sensor_data["sensor_4"][-1]
    ]

    # Voeg toe aan de bewegingsgeschiedenis
    sensor_history.append(latest_values)
    beweging = analyseer_beweging(sensor_history)

    # Genereer de grafiek
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

    return fig, beweging

# === API endpoint om sensor data te ontvangen via POST request ===
@app.route('/data', methods=['POST'])
def receive_data():
    if not request.is_json:
        return jsonify({"status": "error", "message": "Inhoud is geen JSON"}), 400

    try:
        data = request.get_json()
        timestamp = time.strftime("%H:%M:%S")
        current_readings = []

        # Verwerk data per sensor
        for i in range(1, 5):
            key = f"sensor_{i}"
            value = data.get(key)
            if value is None or not isinstance(value, (int, float)):
                return jsonify({"status": "error", "message": f"Foutieve waarde voor {key}"}), 400

            sensor_data[key].append(value)
            current_readings.append(value)

            # Beperk de hoeveelheid opgeslagen data
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

# === Start de Flask server in een aparte thread, zodat Dash parallel draait ===
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# === Startpunt van de applicatie ===
if __name__ == '__main__':
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()
    dash_app.run_server(debug=True, port=8050, use_reloader=False)