import os
os.environ["DATABASE_URL"] = "sqlite:///./test_arcade.db"
os.environ["ADMIN_API_KEY"] = "test-admin-secret"
from fastapi.testclient import TestClient
from app.main import app

def test_registration_bootstrap_and_feature_flag():
    with TestClient(app) as client:
        assert client.get('/health').status_code == 200
        assert client.get('/api/v1/admin/devices').status_code == 401
        admin = {'X-Admin-Key': 'test-admin-secret'}
        r = client.post('/api/v1/admin/devices', headers=admin, json={'name':'Test CM5'})
        assert r.status_code == 201
        obj = r.json()
        device = {'Authorization': 'Bearer ' + obj['device_token']}
        r = client.get('/api/v1/device/bootstrap', headers=device)
        assert r.status_code == 200
        assert r.json()['settings']['theme'] == 'retro-arcade'
        assert r.json()['features']['retro_games'] is False
        assert client.patch('/api/v1/admin/features/retro_games', headers=admin, json={'enabled': True}).status_code == 200
        assert client.get('/api/v1/device/bootstrap', headers=device).json()['features']['retro_games'] is True
        assert client.post('/api/v1/device/heartbeat', headers=device, json={'firmware':'0.1.0'}).status_code == 200
        assert client.patch(f"/api/v1/admin/devices/{obj['id']}/settings", headers=admin, json={'theme':'minimal-tv'}).status_code == 200
        assert client.get('/api/v1/device/bootstrap', headers=device).json()['settings']['theme'] == 'minimal-tv'
