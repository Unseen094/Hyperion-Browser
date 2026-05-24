import asyncio
import json
import websockets


class DevToolsBackend:
    def __init__(self):
        self.sessions = {}

    async def handle(self, websocket, path):
        async for msg in websocket:
            data = json.loads(msg)
            result = {"id": data.get("id"), "result": {}}
            await websocket.send(json.dumps(result))

    def run(self, host="localhost", port=9222):
        asyncio.run(websockets.serve(self.handle, host, port))


if __name__ == "__main__":
    DevToolsBackend().run()
