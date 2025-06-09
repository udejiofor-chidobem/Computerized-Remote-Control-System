import serial
import tkinter as tk
from tkinter import scrolledtext
import threading

# Open serial connection (update COM port as needed)
ser = serial.Serial("COM8", 115200, timeout=1)

def send_command(cmd):
    """Send command to the Arduino Mega over serial and log it."""
    ser.write(f"{cmd}\n".encode())
    log_message(f"CMD Sent: {cmd}")

def read_serial():
    """Continuously read from the Arduino and display received messages."""
    while True:
        if ser.in_waiting:
            try:
                message = ser.readline().decode().strip()
                if message:
                    log_message(f"MCU: {message}")
            except Exception as e:
                log_message(f"CMD Error: {str(e)}")

def log_message(message):
    """Log messages in the console text area."""
    console_text.config(state=tk.NORMAL)
    console_text.insert(tk.END, message + "\n")
    console_text.config(state=tk.DISABLED)
    console_text.yview(tk.END)  # Auto-scroll to the bottom

def update_pwm(value):
    """Send PWM command when slider is moved."""
    send_command(f"PWM:{int(value)}")

def send_manual_command():
    """Send command typed in the console input box."""
    cmd = console_input.get()
    if cmd:
        send_command(cmd)
        console_input.delete(0, tk.END)

# Create the Tkinter window
root = tk.Tk()
root.title("CRCS Controller")

# PWM Slider
tk.Label(root, text="PWM Value").pack()
pwm_slider = tk.Scale(root, from_=0, to=100, orient=tk.HORIZONTAL, command=update_pwm)
pwm_slider.pack()

# Buttons for sending commands
tk.Button(root, text="Stop", command=lambda: send_command("STOP")).pack()
tk.Button(root, text="Start", command=lambda: send_command("START")).pack()

# Console output (Scrollable Text)
console_text = scrolledtext.ScrolledText(root, height=10, width=50, state=tk.DISABLED)
console_text.pack()

# Manual Command Input
console_input = tk.Entry(root, width=50)
console_input.pack()
tk.Button(root, text="Send Command", command=send_manual_command).pack()

# Start a background thread for reading serial data
serial_thread = threading.Thread(target=read_serial, daemon=True)
serial_thread.start()

# Run the Tkinter GUI
root.mainloop()
