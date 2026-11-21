"""Reference client for the CM5; integrate via device daemon later."""
import json
import os
import urllib.request

url = os.environ["ARCADE_API_URL"].rstrip("/")
token = os.environ["ARCADE_DEVICE_TOKEN"]
req = urllib.request.Request(url + "/api/v1/device/bootstrap", headers={"Authorization": "Bearer " + token})
with urllib.request.urlopen(req, timeout=10) as response:
    data = json.load(response)
print(json.dumps(data, indent=2))
