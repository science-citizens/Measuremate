# Server with graphical representation of sonar data
# This server receives data from sonar sensors and displays it in a bar chart using Dash and Plotly.

from flask import Flask, request, jsonify
import dash
from dash import dcc, html
from dash.dependencies import Input, Output
import plotly.graph_objs as go
import threading
import time

app = Flask(__name__)

# Store sonar data
times = []
sensor_1_data = []
sensor_2_data = []
sensor_3_data = []

# Dash app setup
dash_app = dash.Dash(__name__, server=app, routes_pathname_prefix='/dashboard/')

dash_app.layout = html.Div([
    html.H1("Live Sonar Sensor Data"),
    dcc.Graph(id='live-graph'),
    dcc.Interval(id='interval-update', interval=1000, n_intervals=0)  # Update every second
])

@dash_app.callback(Output('live-graph', 'figure'), [Input('interval-update', 'n_intervals')])
def update_graph(n):
    """ Updates the bar chart with the latest sensor data. """
    global sensor_1_data, sensor_2_data, sensor_3_data

    if not sensor_1_data:
        distances = [0, 0, 0]  # Default values if no data yet
    else:
        distances = [sensor_1_data[-1], sensor_2_data[-1], sensor_3_data[-1]]  # Latest values

    return {
        'data': [
            go.Bar(x=["Sensor 1", "Sensor 2", "Sensor 3"], y=distances, marker=dict(color=['red', 'blue', 'green']))
        ],
        'layout': go.Layout(
            title='Sonar Sensor Readings',
            xaxis={'title': 'Sensors'},
            yaxis={'title': 'Distance (cm)', 'range': [0, max(distances) + 10]},
            bargap=0.5
        )
    }


@app.route('/data', methods=['POST'])
def receive_data():
    """ Receives sonar data and updates the stored values. """
    global times, sensor_1_data, sensor_2_data, sensor_3_data
    try:
        data = request.json
        current_time = time.strftime("%H:%M:%S")

        # Append new data, keeping only the last 20 values
        times.append(current_time)
        sensor_1_data.append(data["sensor_1"])
        sensor_2_data.append(data["sensor_2"])
        sensor_3_data.append(data["sensor_3"])

        if len(times) > 20:
            times.pop(0)
            sensor_1_data.pop(0)
            sensor_2_data.pop(0)
            sensor_3_data.pop(0)

        print(f"Received Data: {data}")
        return jsonify({"status": "success", "received": data})

    except Exception as e:
        return jsonify({"status": "error", "message": str(e)}), 400

def run_flask():
    """ Runs Flask in a separate thread. """
    app.run(host='0.0.0.0', port=5000, debug=True, use_reloader=False)

if __name__ == '__main__':
    threading.Thread(target=run_flask).start()
    dash_app.run_server(debug=True, port=8050, use_reloader=False)
