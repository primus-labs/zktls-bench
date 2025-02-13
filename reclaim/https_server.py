import http.server
import ssl


class SimpleHTTPSRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        if self.path == "/" or self.path == "/index.html":
            self.send_header("Content-type", "text/html")
            self.end_headers()
            with open("./browser/index.html", "r") as f:
                content = f.read()
                self.wfile.write(content.encode("utf-8"))
        elif self.path.find("/resources/") != -1 and self.path.find(".js") != -1:
            self.send_header("Content-type", "application/javascript")
            self.end_headers()
            with open(f"./browser{self.path}", "r") as f:
                content = f.read()
                self.wfile.write(content.encode("utf-8"))
        else:
            self.send_header("Content-type", "text/html")
            self.end_headers()
            self.wfile.write(b"s")


httpd = http.server.HTTPServer(("0.0.0.0", 8000), SimpleHTTPSRequestHandler)

context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
# openssl req -x509 -newkey rsa:4096 -nodes -out cert.pem -keyout key.pem -days 365
context.load_cert_chain(certfile="cert.pem", keyfile="key.pem")

httpd.socket = context.wrap_socket(httpd.socket, server_side=True)

print(f"Serving HTTPS on port 8000")
httpd.serve_forever()
