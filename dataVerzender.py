import requests
import time
import random

# Serveradres
url = "http://localhost:5000/data"

datas = [
    {"sensor_1": 52, "sensor_2": 50, "sensor_3": 49, "sensor_4": 51},
    {"sensor_1": 41, "sensor_2": 39, "sensor_3": 40, "sensor_4": 42},
    {"sensor_1": 29, "sensor_2": 30, "sensor_3": 30, "sensor_4": 32},
    {"sensor_1": 21, "sensor_2": 19, "sensor_3": 20, "sensor_4": 22},
    {"sensor_1": 11, "sensor_2": 10, "sensor_3": 9, "sensor_4": 10},
    {"sensor_1": 22, "sensor_2": 20, "sensor_3": 20, "sensor_4": 20},
    {"sensor_1": 29, "sensor_2": 31, "sensor_3": 30, "sensor_4": 31},
    {"sensor_1": 39, "sensor_2": 40, "sensor_3": 40, "sensor_4": 40},
    {"sensor_1": 52, "sensor_2": 51, "sensor_3": 50, "sensor_4": 50},
    {"sensor_1": 51, "sensor_2": 49, "sensor_3": 50, "sensor_4": 41},
    {"sensor_1": 50, "sensor_2": 51, "sensor_3": 51, "sensor_4": 29},
    {"sensor_1": 51, "sensor_2": 52, "sensor_3": 39, "sensor_4": 30},
    {"sensor_1": 52, "sensor_2": 49, "sensor_3": 31, "sensor_4": 30},
    {"sensor_1": 30, "sensor_2": 31, "sensor_3": 50, "sensor_4": 52},
    {"sensor_1": 29, "sensor_2": 30, "sensor_3": 50, "sensor_4": 51},
    {"sensor_1": 29, "sensor_2": 30, "sensor_3": 50, "sensor_4": 51},
    {"sensor_1": 29, "sensor_2": 40, "sensor_3": 51, "sensor_4": 49},
]

for i in range(17):
    data = datas[i]
    print(f"Verzenden van data: {data}")
    response = requests.post(url, json=data)
    print(f"Status code: {response.status_code}, Response: {response.json()}")
    time.sleep(0.5)