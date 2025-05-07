from flask import Flask, request, jsonify
import dash
import statistics
from dash import dcc, html
from dash.dependencies import Input, Output
import plotly.graph_objs as go
import threading
import time

# Flask app
app = Flask(__name__)

# Global variable for storing the latest classification
latest_classification = "Nog geen data ontvangen"

# Storage for sonar data
sensor_data = {
    "sensor_1": [],
    "sensor_2": [],
    "sensor_3": [],
    "sensor_4": [],
    "timestamps": []
}

def classify_readings(sensor_values, near_threshold=30, equal_margin=10):
    """
    Classificeer de huidige meting van 4 sonars
    :param sensor_values: lijst van 4 afstandsmetingen
    :param near_threshold: afstand die als 'dichtbij' telt
    :param equal_margin: verschil waarbij we afstanden als 'gelijk' zien
    :return: classificatie (string), indices van sensoren die dichtbij iets zien
    """
    if len(sensor_values) != 4:
        return "Ongeldige invoer", []

    # Dichtbij = kleiner dan een drempel
    near_indices = [i for i, v in enumerate(sensor_values) if v < near_threshold]

    # Check of alle metingen ongeveer gelijk zijn
    if max(sensor_values) - min(sensor_values) < equal_margin:
        print("Alle sensoren zien ongeveer hetzelfde")
        return "Groot/breed object (alle sensoren zien ongeveer hetzelfde)"

    if len(near_indices) == 1:
        print("Eén sensor ziet iets")
        return f"Object bij sensor {near_indices[0] + 1}"
    elif 1 < len(near_indices) < 4:
        print("Meerdere sensoren zien iets")
        return f"Meervoudige detectie: {len(near_indices)} sensoren zien iets"
    elif len(near_indices) == 0:
        print("Geen sensoren zien iets")
        return "Geen object zichtbaar", []
    else:
        print("Onbekende situatie")
        return "Onbekende situatie"

# Dash app for live visualization
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

dash_app.layout = html.Div([
    html.H2("Live Sonar Sensor Readings"),
    dcc.Graph(id='live-graph'),
    html.Div(id='classification-output', style={'fontSize': 20, 'marginTop': '20px', 'fontWeight': 'bold'}),
    dcc.Interval(id='interval-update', interval=500, n_intervals=0)  # update every 2 seconds
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

    # Update classification
    latest_classification = classify_readings(latest_values)

    # Build figure
    fig = go.Figure(
        data=[go.Scatter(
            x=["Sensor 1", "Sensor 2", "Sensor 3", "Sensor 4"],
            y=latest_values,
            mode='lines+markers',
            line=dict(color='royalblue', width=3),
            marker=dict(size=8)
        )],
        layout=go.Layout(
            title='Latest Sonar Sensor Values',
            xaxis={'title': 'Sensors'},
            yaxis={'title': 'Distance (cm)', 'range': [0, max(latest_values) + 20]}
        )
    )

    return fig, latest_classification

@app.route('/data', methods=['POST'])
def receive_data():
    try:
        data = request.json
        timestamp = time.strftime("%H:%M:%S")

        # Verzamel actuele sensordata in een lijst
        current_readings = []
        for i in range(1, 5):
            sensor_key = f"sensor_{i}"
            if sensor_key in data:
                sensor_data[sensor_key].append(data[sensor_key])
                current_readings.append(data[sensor_key])
                if len(sensor_data[sensor_key]) > 20:
                    sensor_data[sensor_key].pop(0)

        sensor_data["timestamps"].append(timestamp)
        if len(sensor_data["timestamps"]) > 20:
            sensor_data["timestamps"].pop(0)

        print(f"[{timestamp}] Received: {data}")
        return jsonify({
            "status": "success",
            "received": data
        })

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

# Threaded Flask launch
def run_flask():
    app.run(host='0.0.0.0', port=5000, debug=False, use_reloader=False)

# Main
if __name__ == '__main__':
    threading.Thread(target=run_flask).start()
    dash_app.run_server(debug=True, port=8050, use_reloader=False)
