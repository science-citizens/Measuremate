from dash.dependencies import Input, Output
from flask import Flask, request, jsonify
import plotly.graph_objs as go
from collections import deque
from dash import dcc, html
import threading
import dash
import time

# Initialiseer Flask-app voor het ontvangen van data via HTTP
app = Flask(__name__)

# Initialiseer Dash-app voor live visualisatie in de browser
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

# Configuratie en drempelwaarden
MAX_HISTORY = 5               # Aantal tijdstappen voor visualisatie
MAX_VALUES = 20               # Max aantal waarden per sensor om op te slaan
GRADIENT_THRESHOLD = 5        # Drempel voor hellingclassificatie
MOVEMENT_THRESHOLD = 4        # Drempel voor bewegingsanalyse
NEAR_THRESHOLD = 38           # Afstand waarbij object als "dichtbij" wordt beschouwd
EQUAL_MARGIN = 3              # Acceptabel verschil voor "gelijke afstand"
SENSOR_POSITIONS = [0, 17, 26, 43]  # Afstanden tussen de sensoren in cm

# Opslagstructuur voor sensordata
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

# Tijdgeschiedenis van metingen
sensor_history = deque(maxlen=2)

# Functie om beweging over tijd te analyseren
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

    # Simpele regelgebaseerde classificatie van beweging
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

# Analyseer helling (gradient) over sensoren
def analyseer_gradient(sensor_values, threshold=GRADIENT_THRESHOLD):
    if len(sensor_values) != 4:
        return "Ongeldige invoer"

    gradients = [sensor_values[i+1] - sensor_values[i] for i in range(3)]

    if all(abs(g) < threshold for g in gradients):
        return "Object recht voor het systeem"
    elif (sensor_values[2] < NEAR_THRESHOLD and sensor_values[3] < NEAR_THRESHOLD):
        return "Object aan rechterkant voor het systeem"
    elif (sensor_values[0] < NEAR_THRESHOLD and sensor_values[1] < NEAR_THRESHOLD):
        return "Object aan linkerkant voor het systeem"
    elif gradients[1] < -threshold and gradients[2] < -threshold:
        return "Object komt schuin van rechts"
    elif gradients[0] > threshold and gradients[1] > threshold:
        return "Object komt schuin van links"
    elif gradients[0] < -threshold and gradients[1] > threshold:
        return "Object schuin rechtsvoor"
    elif gradients[0] > threshold and gradients[1] < -threshold:
        return "Object schuin linksvoor"
    else:
        return "Onregelmatig object of meerdere reflecties"

# Positieanalyse op basis van afstand en spreiding
def analyseer_posities(sensor_values, near_threshold=NEAR_THRESHOLD, equal_margin=EQUAL_MARGIN):
    if len(sensor_values) != 4:
        return "Ongeldige invoer"

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
            return "Alle sensoren volledige dekking (minimale breedte: 43 cm)"
        else:
            return "Breed object op afstand"

    if not near_indices:
        return "Geen object zichtbaar"

    pos_names = ["links", "midden-links", "midden-rechts", "rechts"]
    if len(near_indices) == 1:
        return f"Object dichtbij bij sensor {near_indices[0] + 1} ({pos_names[near_indices[0]]})"

    breedte = geschatte_breedte(near_indices)
    beschrijving = {
        2: "Twee sensoren – gedeeltelijk object",
        3: "Drie sensoren – waarschijnlijk breed object",
        4: "Alle sensoren – volledige dekking"
    }.get(len(near_indices), "Onbekende situatie")

    return f"{beschrijving} (geschatte breedte: {breedte} cm)"

# Dash layout configuratie
dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=500, n_intervals=0)
])

# Dash callback om data en classificatie weer te geven
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

    positie_info = analyseer_posities(latest_values)
    helling_info = analyseer_gradient(latest_values)
    beweging_info = analyseer_beweging(sensor_history)

    combined_output = f"{positie_info} | {helling_info} | {beweging_info}"

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

    return fig, combined_output

# Flask route voor het ontvangen van sensordata via HTTP POST
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

# Draai de Flask-server in een aparte thread zodat Dash parallel draait
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# Start zowel Flask als Dash
if __name__ == '__main__':
    flask_thread = threading.Thread(target=run_flask, daemon=True)
    flask_thread.start()
    dash_app.run_server(debug=True, port=8050, use_reloader=False)