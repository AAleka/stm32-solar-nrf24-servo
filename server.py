from fastapi import FastAPI, HTTPException
from pyrf24 import RF24, RF24_250KBPS, RF24_PA_MAX
import logging
import time

CE_PIN = 22
CSN_PIN = 0
CHANNEL = 76
ADDRESSES = [b"1Node", b"2Node"]

app = FastAPI(title="RF24 Service", version="1.0.0")
logger = logging.getLogger("uvicorn")
radio: RF24 | None = None


def send_message(message: str) -> str:
    if not radio:
        raise RuntimeError("RF24 not initialized")

    payload = message.encode("utf-8")
    logger.info(f"Sending: {message}")
    radio.stop_listening()

    ok = radio.write(payload)
    if not ok:
        raise RuntimeError("Failed to send payload")

    radio.start_listening()
    started_waiting = time.monotonic()
    timeout = 3.0
    received = None

    while (time.monotonic() - started_waiting) < timeout:
        if radio.available():
            received = radio.read(32)
            break

    radio.stop_listening()
    if received:
        reply = received.decode("utf-8").rstrip("\x00").strip()
        logger.info(f"💬 Reply: {reply}")
        return reply
    else:
        logger.warning("⚠️ No reply received.")
        return "No reply"


@app.on_event("startup")
def startup_event():
    global radio
    logger.info("Initializing RF24 radio...")

    radio = RF24(CE_PIN, CSN_PIN, 10_000_000)
    if not radio.begin():
        logger.error("RF24 hardware not responding. Check wiring and power.")
        raise RuntimeError("RF24 initialization failed")

    radio.setPALevel(RF24_PA_MAX)
    radio.setDataRate(RF24_250KBPS)
    radio.setChannel(CHANNEL)
    radio.openReadingPipe(1, ADDRESSES[0])
    radio.openWritingPipe(ADDRESSES[1])
    radio.stop_listening()

    logger.info("RF24 initialized successfully.")


@app.on_event("shutdown")
def shutdown_event():
    global radio
    if radio:
        logger.info("Shutting down RF24 service.")
        radio.powerDown()
        radio = None


@app.get("/")
def root():
    return {"service": "rf24", "status": "running"}


@app.get("/on")
def turn_on():
    try:
        reply = send_message("on")
        return {"status": "ok", "message": "LED turned on", "reply": reply}
    except Exception as e:
        logger.exception("Error sending 'on'")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/off")
def turn_off():
    try:
        reply = send_message("off")
        return {"status": "ok", "message": "LED turned off", "reply": reply}
    except Exception as e:
        logger.exception("Error sending 'off'")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/servo/{angle}")
def move_servo(angle: int):
    if not 0 <= angle <= 180:
        raise HTTPException(status_code=400, detail="Angle must be between 0 and 180")
    try:
        reply = send_message(f"servo {angle}")
        return {"status": "ok", "message": f"Servo moved to {angle}°", "reply": reply}
    except Exception as e:
        logger.exception("Error sending servo command")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/btlvl")
def bt_lvl():
    try:
        reply = send_message("btlvl")
        return {"status": "ok", "message": "Battery level", "reply": reply}
    except Exception as e:
        logger.exception("Error sending btlvl command")
        raise HTTPException(status_code=500, detail=str(e))


@app.get("/rdoff/{minutes}")
def rd_off(minutes: int):
    if not 0 <= minutes <= 720:
        raise HTTPException(status_code=400, detail="Minutes must be between 0 and 720")
    try:
        reply = send_message(f"rdoff {minutes}")
        return {"status": "ok", "message": f"RD off for {minutes} min", "reply": reply}
    except Exception as e:
        logger.exception("Error sending 'rdoff'")
        raise HTTPException(status_code=500, detail=str(e))