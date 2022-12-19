#! /usr/bin/python3

from playwright.sync_api import sync_playwright
import yaml
from threading import Thread
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from urllib.parse import parse_qs

config = {}

def renderer():
    with sync_playwright() as p:
        browser = p.chromium.launch()
        for _, screen in config["screens"].items(): 
            page = browser.new_page(has_touch=True, is_mobile=True, extra_http_headers={"Authorization": screen["authorization"]})
            page.set_viewport_size({"width": screen["width"], "height": screen["height"]})
            page.goto(screen["url"])
            screen["page"] = page
        while True:
            for _, screen in config["screens"].items():
                page = screen["page"]
                if "click" in screen:
                    print(screen["click"])
                    page.touchscreen.tap(screen["click"]["x"], screen["click"]["y"])
                    del screen["click"]
                screen["screenshot"] = page.screenshot(type="jpeg")
        browser.close()


class ESPanelServer(BaseHTTPRequestHandler):
    def do_GET(self):
        path = self.path.strip("/")
        if path not in config["screens"]:
            self.send_response(404)
            self.end_headers()
            return
        self.send_response(200)
        self.send_header("Content-type", "image/jpeg")
        self.end_headers()
        self.wfile.write(config["screens"][path]["screenshot"])
    
    def do_POST(self):
        path = self.path.strip("/")
        if path not in config["screens"]:
            self.send_response(404)
            self.end_headers()
            return
        content_len = int(self.headers.get('Content-Length'))
        request = parse_qs(self.rfile.read(content_len))
        print(request[b'x'])
        config["screens"][path]["click"] = {"x": int(request[b'x'][0]), "y": int(request[b'y'][0])}
        self.send_response(200)
        self.end_headers()

if __name__ == "__main__":
    with open("config.yml", 'r') as config_file:
        try:
            config = yaml.safe_load(config_file)
            print(config)
        except yaml.YAMLError as e:
            print(e)

    Thread(target=renderer).start()
    server = ThreadingHTTPServer((config["server"]["host"], config["server"]["port"]), ESPanelServer)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass

    server.server_close()
    print("Server stopped.")
    