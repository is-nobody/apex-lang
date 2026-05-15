# source/libraries/network_module.py
import urllib.parse as _urlparse
import urllib.request
import urllib.error
import socket
import json
import select
from .builtin_module import BuiltinModule


class Network(BuiltinModule):
    def __init__(self):
        super().__init__("network")
        self._funcs = {
            # URL functions
            "url_encode": self.url_encode,
            "url_decode": self.url_decode,
            "parse_url": self.parse_url,
            
            # HTTP functions
            "http_get": self.http_get,
            "http_post": self.http_post,
            "http_set_timeout": self.http_set_timeout,
            
            # TCP functions
            "tcp_connect": self.tcp_connect,
            "socket_send": self.socket_send,
            "socket_receive": self.socket_receive,
            "socket_close": self.socket_close,
            "socket_set_timeout": self.socket_set_timeout,
            
            "tcp_listen": self.tcp_listen,
            "server_accept": self.server_accept,
            "server_close": self.server_close,
            
            # WebSocket functions
            "websocket_connect": self.websocket_connect,
            "ws_send": self.ws_send,
            
            # DNS functions
            "dns_lookup": self.dns_lookup,
            
            # IP functions
            "ip_is_valid": self.ip_is_valid,
        }
        # Store active sockets and servers
        self._sockets = {}
        self._servers = {}
        self._websockets = {}
        self._socket_counter = 0
        self._timeout = 30  # default timeout in seconds
    
    # ========== URL Functions ==========
    
    @staticmethod
    def url_encode(s: str) -> str:
        """URL-encodes a string"""
        try:
            return _urlparse.quote(s, safe='')
        except Exception:
            return s
    
    @staticmethod
    def url_decode(s: str) -> str:
        """URL-decodes a string"""
        try:
            return _urlparse.unquote(s)
        except Exception:
            return s
    
    @staticmethod
    def parse_url(url: str):
        """Parses URL into components and returns a table"""
        try:
            parsed = _urlparse.urlparse(url)
            from source.core.interpreter import Table
            result = Table()
            result.set("scheme", parsed.scheme)
            result.set("host", parsed.hostname)
            result.set("port", parsed.port)
            result.set("path", parsed.path)
            result.set("query", parsed.query)
            result.set("fragment", parsed.fragment)
            return result
        except Exception:
            return None
    
    # ========== HTTP Functions ==========
    
    @staticmethod
    def http_get(url: str, headers=None) -> str:
        """Performs HTTP GET request"""
        try:
            req = urllib.request.Request(url)
            if headers:
                if isinstance(headers, dict):
                    for key, value in headers.items():
                        req.add_header(key, str(value))
                else:
                    # Try to treat as Table
                    try:
                        items = headers.items
                        for key in items:
                            req.add_header(str(key), str(items[key]))
                    except Exception:
                        pass
            
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.read().decode('utf-8')
        except urllib.error.HTTPError as e:
            return f"HTTP Error: {e.code} - {e.reason}"
        except urllib.error.URLError as e:
            return f"URL Error: {e.reason}"
        except Exception as e:
            return f"Error: {e}"
    
    @staticmethod
    def http_post(url: str, body=None, headers=None) -> str:
        """Performs HTTP POST request"""
        try:
            # Convert body to bytes
            if body is None:
                data = b''
            elif isinstance(body, str):
                data = body.encode('utf-8')
            elif isinstance(body, dict):
                data = json.dumps(body).encode('utf-8')
            else:
                data = str(body).encode('utf-8')
            
            req = urllib.request.Request(url, data=data, method='POST')
            req.add_header('Content-Type', 'application/json')
            
            if headers:
                if isinstance(headers, dict):
                    for key, value in headers.items():
                        req.add_header(key, str(value))
                else:
                    # Try to treat as Table
                    try:
                        items = headers.items
                        for key in items:
                            req.add_header(str(key), str(items[key]))
                    except Exception:
                        pass
            
            with urllib.request.urlopen(req, timeout=30) as response:
                return response.read().decode('utf-8')
        except urllib.error.HTTPError as e:
            return f"HTTP Error: {e.code} - {e.reason}"
        except urllib.error.URLError as e:
            return f"URL Error: {e.reason}"
        except Exception as e:
            return f"Error: {e}"
    
    def http_set_timeout(self, seconds: float):
        """Sets timeout for HTTP requests"""
        self._timeout = seconds
        socket.setdefaulttimeout(seconds)
    
    # ========== TCP Socket Functions ==========
    
    def tcp_connect(self, host: str, port: int):
        """Creates TCP connection to host:port"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(self._timeout)
            sock.connect((host, port))
            
            self._socket_counter += 1
            socket_id = self._socket_counter
            self._sockets[socket_id] = sock
            
            from source.core.interpreter import Table
            result = Table()
            result.set("id", socket_id)
            result.set("connected", True)
            result.set("host", host)
            result.set("port", port)
            return result
        except Exception as e:
            return None
    
    def socket_send(self, socket_id: int, data: str) -> bool:
        """Sends data through socket"""
        try:
            if socket_id not in self._sockets:
                return False
            sock = self._sockets[socket_id]
            sock.send(data.encode('utf-8'))
            return True
        except Exception:
            return False
    
    def socket_receive(self, socket_id: int, bytes_count: int = 4096) -> str:
        """Receives data from socket"""
        try:
            if socket_id not in self._sockets:
                return None
            sock = self._sockets[socket_id]
            data = sock.recv(bytes_count)
            return data.decode('utf-8')
        except socket.timeout:
            return None
        except Exception:
            return None
    
    def socket_close(self, socket_id: int = None):
        """Closes a socket or all sockets"""
        if socket_id is None:
            # Close all sockets
            for sock in self._sockets.values():
                try:
                    sock.close()
                except Exception:
                    pass
            self._sockets.clear()
        elif socket_id in self._sockets:
            try:
                self._sockets[socket_id].close()
            except Exception:
                pass
            del self._sockets[socket_id]
    
    def socket_set_timeout(self, seconds: float):
        """Sets timeout for socket operations"""
        self._timeout = seconds
        for sock in self._sockets.values():
            try:
                sock.settimeout(seconds)
            except Exception:
                pass
    
    # ========== TCP Server Functions ==========
    
    def tcp_listen(self, port: int, backlog: int = 5):
        """Creates TCP server listening on port"""
        try:
            server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            server.bind(('0.0.0.0', port))
            server.listen(backlog)
            server.settimeout(self._timeout)
            
            self._socket_counter += 1
            server_id = self._socket_counter
            self._servers[server_id] = server
            
            from source.core.interpreter import Table
            result = Table()
            result.set("id", server_id)
            result.set("port", port)
            result.set("listening", True)
            return result
        except Exception as e:
            return None
    
    def server_accept(self, server_id: int):
        """Accepts incoming connection on server"""
        try:
            if server_id not in self._servers:
                return None
            server = self._servers[server_id]
            client, address = server.accept()
            client.settimeout(self._timeout)
            
            self._socket_counter += 1
            client_id = self._socket_counter
            self._sockets[client_id] = client
            
            from source.core.interpreter import Table
            result = Table()
            result.set("id", client_id)
            result.set("address", f"{address[0]}:{address[1]}")
            result.set("port", address[1])
            result.set("host", address[0])
            return result
        except socket.timeout:
            return None
        except Exception:
            return None
    
    def server_close(self, server_id: int = None):
        """Closes a server or all servers"""
        if server_id is None:
            # Close all servers
            for server in self._servers.values():
                try:
                    server.close()
                except Exception:
                    pass
            self._servers.clear()
        elif server_id in self._servers:
            try:
                self._servers[server_id].close()
            except Exception:
                pass
            del self._servers[server_id]
    
    # ========== WebSocket Functions ==========
    
    def websocket_connect(self, url: str):
        """Creates WebSocket connection (simplified - requires websocket-client library)"""
        try:
            # Try to import websocket module
            try:
                import websocket
            except ImportError:
                print("Warning: WebSocket support requires 'websocket-client' library")
                print("Install with: pip install websocket-client")
                return None
            
            ws = websocket.create_connection(url, timeout=self._timeout)
            
            self._socket_counter += 1
            ws_id = self._socket_counter
            self._websockets[ws_id] = ws
            
            from source.core.interpreter import Table
            result = Table()
            result.set("id", ws_id)
            result.set("connected", True)
            result.set("url", url)
            return result
        except Exception as e:
            return None
    
    def ws_send(self, ws_id: int, data: str) -> bool:
        """Sends data through WebSocket"""
        try:
            if ws_id not in self._websockets:
                return False
            ws = self._websockets[ws_id]
            ws.send(data)
            return True
        except Exception:
            return False
    
    def ws_receive(self, ws_id: int) -> str:
        """Receives data from WebSocket"""
        try:
            if ws_id not in self._websockets:
                return None
            ws = self._websockets[ws_id]
            return ws.recv()
        except Exception:
            return None
    
    def ws_close(self, ws_id: int = None):
        """Closes WebSocket connection"""
        if ws_id is None:
            for ws in self._websockets.values():
                try:
                    ws.close()
                except Exception:
                    pass
            self._websockets.clear()
        elif ws_id in self._websockets:
            try:
                self._websockets[ws_id].close()
            except Exception:
                pass
            del self._websockets[ws_id]
    
    # ========== DNS Functions ==========
    
    @staticmethod
    def dns_lookup(hostname: str) -> str:
        """Resolves hostname to IP address"""
        try:
            return socket.gethostbyname(hostname)
        except Exception:
            return None
    
    # ========== IP Functions ==========
    
    @staticmethod
    def ip_is_valid(ip: str) -> bool:
        """Validates IPv4 or IPv6 address"""
        try:
            socket.inet_pton(socket.AF_INET, ip)
            return True
        except (socket.error, ValueError):
            try:
                socket.inet_pton(socket.AF_INET6, ip)
                return True
            except (socket.error, ValueError):
                return False