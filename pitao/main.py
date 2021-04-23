import requests
import matplotlib.pyplot as plt

#Python script for debugging file length
r = requests.get("http://hass.local:8123/api/media_player_proxy/media_player.spotify_wesley_gabriel?token=7dc63fe15425851e09e1521d9ddae50f58f8981330a851c79cc97bde512c48bf&cache=b869390ec70354da")

img = r.content

print(len(img))