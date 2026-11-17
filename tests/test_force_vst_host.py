#!/usr/bin/env python3
"""Local fixture exercises worker reconnect and shared polling without a UMIK or Force."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import os, subprocess, sys, threading, urllib.request

PORT=18096
hits=0
class Handler(BaseHTTPRequestHandler):
    protocol_version="HTTP/1.1"
    def log_message(self,*args): pass
    def do_GET(self):
        global hits
        if self.path != "/api/spectrum": self.send_error(404); return
        hits += 1
        if hits == 1:
            self.send_response(503); self.send_header("Content-Length","0"); self.send_header("Connection","close"); self.end_headers(); self.close_connection=True; return
        levels=','.join(['-30']*31); peaks=','.join(['-20']*31)
        body=(f'{{"ok":true,"rms_dbfs":-20,"z_corrected_dbfs":-20,"a_corrected_dbfs":-20,'
              f'"orientation":0,"calibrated":false,"sensitivity_factor_db":2.7673,"spl_z":null,"spl_a":null,'
              f'"spl_reference_type":"none","level":[{levels}],"peak":[{peaks}]}}').encode()
        self.send_response(200); self.send_header("Content-Type","application/json")
        self.send_header("Content-Length",str(len(body))); self.end_headers(); self.wfile.write(body)
    def do_POST(self): self.send_response(200); self.send_header("Content-Length","2"); self.end_headers(); self.wfile.write(b"{}")

def main():
    if len(sys.argv) not in (3,4): return 2
    server=ThreadingHTTPServer(("127.0.0.1",PORT),Handler)
    thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
    env=os.environ.copy();env["TROLLFX_RTA_PORT"]=str(PORT)
    try:
        result=subprocess.run([sys.argv[1],sys.argv[2]],env=env,check=False)
        if result.returncode==0 and len(sys.argv)==4:
            result=subprocess.run([sys.argv[1],sys.argv[2],"--saved-state-compat",sys.argv[3]],env=env,check=False)
        before=hits
        with urllib.request.urlopen(f"http://127.0.0.1:{PORT}/api/spectrum",timeout=2) as response:
            if response.status!=200: raise RuntimeError("external fixture not responding")
        if hits<=before: raise RuntimeError("plugin shutdown affected the externally owned service")
    finally: server.shutdown();server.server_close();thread.join(timeout=2)
    if hits<2: print("FAIL bounded poll/reconnect attempts",hits); return 1
    if hits>14: print("FAIL duplicate poller detected for two instances",hits); return 1
    return result.returncode
if __name__=="__main__": raise SystemExit(main())
