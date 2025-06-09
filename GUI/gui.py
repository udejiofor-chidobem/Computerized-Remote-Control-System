from pathlib import Path
import time
import serial
import serial.tools.list_ports
import struct
import threading
from tkinter import Tk, Canvas, Entry, Text, Button, PhotoImage, Checkbutton, IntVar, scrolledtext, ttk, font
import tkinter as tk
from PIL import Image, ImageDraw, ImageFont, ImageTk
import os
import ctypes

OUTPUT_PATH = Path(__file__).parent
ASSETS_PATH = OUTPUT_PATH / Path(r"assets/frame0")

ser = None

def relative_to_assets(path: str) -> Path:
    return ASSETS_PATH / Path(path)

# Open serial connection (update COM port as needed)
# ser = serial.Serial("COM8", 115200, timeout=1)

def get_com_ports():
    """Get available COM ports"""
    ports = serial.tools.list_ports.comports()
    com_ports = [f"{port.device} - {port.description}" for port in ports]  # Extract the COM port names
    return com_ports

def on_select_com_port(event):
    """Callback when a COM port is selected"""
    global ser
    selected_port = com_port_combobox.get()
    print(f"Selected COM Port: {selected_port}")
    
    # Extract the device name from the selection (e.g., "COM1 - USB Serial Device")
    selected_device = selected_port.split(" -")[0]  # Get the device name part

    # You can open the serial port here using pyserial
    try:
        ser = serial.Serial(selected_device, 115200, timeout=1)  # Adjust baud rate as needed
        time.sleep(2)
        print(f"Connected to {selected_device}")
        serial_thread.start()
        heartbeat_thread.start()
    except serial.SerialException as e:
        print(f"Error connecting to {selected_device}: {e}")
    com_port_combobox.configure(values=get_com_ports())
    
        

def send_command(cmd):
    """Send command to the Arduino Mega over serial and log it."""
    head = cmd[0:3]
    if head == "HRT":
        ser.write(b'\x01\x00\x00\x00')  # Sending heartbeat (COMMAND_ID = 0x01)
    elif head == "PWM":
        payload = [cmd.split(':')[1], 0, 0]
        payload = struct.pack("BBBB", 0x02, *list(int(i) for i in payload))
        ser.write(payload)
        print(payload)
    elif head == "RMP":
        payload = struct.pack("BBBB", 0x03, *list(map(int,cmd.split(':')[1:4])))
        ser.write(payload)
        print(payload)
    elif head == "STP":
        payload = struct.pack("BBBB", 0x04, *list(map(int,cmd.split(':')[1:4])))
        ser.write(payload)
        print(payload)
    elif head == "IMP":
        payload = [cmd.split(':')[1], 0, 0]
        payload = struct.pack("BBBB", 0x05, *list(int(i) for i in payload))
        ser.write(payload)
        print(payload)
    elif head == "STO":
        ser.write(b'\x06\x00\x00\x00')
    elif head == "GOO":
        ser.write(b'\x07\x00\x00\x00')
    elif head == "LOG":
        now = time.localtime()
        month = now.tm_mon
        day = now.tm_mday
        hour = now.tm_hour
        min = now.tm_min

        # Pack with opcode (e.g., 0x08 = TIME_SYNC)
        log_message(f"Time Sent: {day}:{hour}:{min}")
        payload = struct.pack("BBBB", 0x08, day, hour, min)
        ser.write(payload)
    else :
        ser.write(f"{cmd}\n".encode())

    log_message(f"CMD Sent: {cmd}")

def send_heartbeat():
    while True:
        # Send heartbeat message to Mega
        send_command("HRT")  # Send as bytes
        print("Heartbeat sent")
        time.sleep(1)

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
    radio_console.config(state=tk.NORMAL)
    radio_console.insert(tk.END, message + "\n")
    radio_console.config(state=tk.DISABLED)
    radio_console.yview(tk.END)  # Auto-scroll to the bottom

def update_text(text : Text, value : str):
    text.config(state="normal")
    text.delete("1.0", "end")  # Clear the text field
    text.insert("1.0", value)  # Insert the new value into the text field
    text.config(state="disabled")

def update_throttle(text : Text, value : str, check: IntVar):
    """Send PWM command when slider is moved."""
    update_text(text, value)
    if check.get():
        update_pwm(value)
    

def update_pwm(value):
    """Send PWM command when slider is moved."""
    send_command(f"PWM:{int(value)}")

def update_imp(value):
    """Send Impulse command when slider is moved."""
    send_command(f"IMP:{int(value)}")

def update_ramp(start, end, time):
    """Send Impulse command when slider is moved."""
    send_command(f"RMP:{int(start)}:{int(end)}:{int(time)}")

def update_step(start, step, end):
    """Send Impulse command when slider is moved."""
    send_command(f"STP:{int(start)}:{int(step)}:{int(end)}")

def send_manual_command():
    """Send command typed in the console input box."""
    cmd = console_input.get()
    if cmd:
        send_command(cmd)
        console_input.delete(0, tk.END)



window = Tk()
window.title("Computerized Remote Control System")

image = Image.new("RGBA", (300, 100), (255, 255, 255, 0))
draw = ImageDraw.Draw(image)

# Load PNG as icon
icon = PhotoImage(file=relative_to_assets("crcs-icon.png"))
window.iconphoto(True, icon)  # Set window icon

window.geometry("1407x791")
window.configure(bg = "#242424")

canvas = Canvas(window,bg = "#242424",height = 791,width = 1407,bd = 0,highlightthickness = 0,relief = "ridge")

canvas.place(x = 0, y = 0)

def load_font(font_path):
    FR_PRIVATE = 0x10
    if os.name == 'nt':
        ctypes.windll.gdi32.AddFontResourceExW(font_path, FR_PRIVATE, 0)

# Path to font
font_path = os.path.abspath(relative_to_assets("Starbirl.otf"))
load_font(font_path)

# 231F20
# Title
canvas.create_text(11.0,0.0,anchor="nw",text="Computerized Remote",fill="#B4A368",font=("Starbirl", 48 * -1))
canvas.create_text(11.0,48.0,anchor="nw",text="Control System ",fill="#B4A368",font=("Starbirl", 48 * -1))

# Text Headers
canvas.create_text(893.0,285.0,anchor="nw",text="radio Console:",fill="#B4A368",font=("Starbirl", 32 * -1))
canvas.create_text(11.0,276.0,anchor="nw",text="RAMP",fill="#B4A368",font=("Starbirl", 24 * -1))
canvas.create_text(11.0,430.0,anchor="nw",text="STEP",fill="#B4A368",font=("Starbirl", 24 * -1))
canvas.create_text(11.0,576.0,anchor="nw",text="IMPULSE",fill="#B4A368",font=("Starbirl", 32 * -1))
canvas.create_text(11.0,133.0,anchor="nw",text="Throttle slider",fill="#B4A368",font=("Starbirl", 24 * -1))

# Console Information
entry_image_1 = PhotoImage(file=relative_to_assets("entry_1.png"))
entry_bg_1 = canvas.create_image(1141.5,438.5,image=entry_image_1)
# radio_console = Text(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0)
radio_console = scrolledtext.ScrolledText(window, height=460, width=225, state=tk.DISABLED)
radio_console.place(x=912.0,y=330.0,width=460.0,height=180.0)

entry_image_2 = PhotoImage(file=relative_to_assets("entry_2.png"))
entry_bg_2 = canvas.create_image(1140.5,535.5,image=entry_image_2)
console_input = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 20 * -1))
console_input.place(x=911.5,y=525.0,width=350.0,height=20.0)

button_image_1 = PhotoImage(file=relative_to_assets("button_1.png"))
console_send = Button(image=button_image_1,borderwidth=0,highlightthickness=0,command=send_manual_command,relief="flat")
console_send.place(x=1265.0,y=513.0,width=114.17906188964844,height=45.17045593261719)


# Throttle Percentage
canvas.create_rectangle(20.0,187.0,667.0,230.0,fill="#D9D9D9",outline="#242424")

continous_output = IntVar()  # Holds 1 (checked) or 0 (unchecked)
checkbox = Checkbutton(window, variable=continous_output, background="#242424")
checkbox.place(x=20.0,y=247.0)

entry_image_3 = PhotoImage(file=relative_to_assets("entry_3.png"))
entry_bg_3 = canvas.create_image(55.5,209.0,image=entry_image_3)
throttleP = Text(bg="#D9D9D9",relief="flat", fg="#000716",highlightthickness=0,font=("Starbirl", 24 * -1))
throttleP.config(state="disabled")
throttleP.place(x=21,y=195.0,width=71.0,height=30)

throttle_slider = tk.Scale(window, from_=0, to=100, orient=tk.HORIZONTAL, bg="#242424",
                           command=lambda value: update_throttle(throttleP, value, continous_output),showvalue=0)
throttle_slider.place(x=100.0,y=198,width=495,height=20)

button_image_3 = PhotoImage(file=relative_to_assets("button_3.png"))
slider_button = Button(image=button_image_3,borderwidth=0,highlightthickness=0,
                       command=lambda: update_pwm(throttle_slider.get()),relief="flat")
slider_button.place(x=598.0,y=182.0,width=213.2259521484375,height=53.45170593261719)


# RAMP Inputs
entry_image_4 = PhotoImage(file=relative_to_assets("entry_4.png"))
entry_bg_4 = canvas.create_image(95.0,386.5,image=entry_image_4)
ramp_start = Entry(bg="#D9D9D9",relief="flat",fg="#000716",highlightthickness=0,font=("Starbirl", 24 * -1))
ramp_start.place(x=53,y=362.0,width=80.0,height=50.0)

entry_image_5 = PhotoImage(file=relative_to_assets("entry_5.png"))
entry_bg_5 = canvas.create_image(264.0,386.5,image=entry_image_5)
ramp_end = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 24 * -1))
ramp_end.place(x=222.0,y=362.0,width=80.0,height=50.0)

ramp_time_I = PhotoImage(file=relative_to_assets("entry_5.png"))
ramp_timeImg = canvas.create_image(464.0,386.5,image=ramp_time_I)
ramp_time = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 24 * -1))
ramp_time.place(x=422.0,y=362.0,width=80.0,height=50.0)

canvas.create_text(32.0,324.0,anchor="nw",text="START",fill="#B4A368",font=("Starbirl", 20 * -1))
canvas.create_text(167.0,348.0,anchor="nw",text=":",fill="#B4A368",font=("Starbirl", 64 * -1))
canvas.create_text(226.0,324.0,anchor="nw",text="END",fill="#B4A368",font=("Starbirl", 20 * -1))
canvas.create_text(426.0,324.0,anchor="nw",text="TIME",fill="#B4A368",font=("Starbirl", 20 * -1))

button_image_4 = PhotoImage(file=relative_to_assets("button_4.png"))
ramp_button = Button(image=button_image_4,borderwidth=0,highlightthickness=0,
                     command=lambda: update_ramp(ramp_start.get(), ramp_end.get(), ramp_time.get()),relief="flat")
ramp_button.place(x=598.0,y=361.0,width=213.2259521484375,height=53.45170593261719)


entry_image_6 = PhotoImage(file=relative_to_assets("entry_6.png"))
entry_bg_6 = canvas.create_image(95.0,538.5,image=entry_image_6)
step_start = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 24 * -1))
step_start.place(x=53.0,y=515,width=80.0,height=50.0)

entry_image_7 = PhotoImage(file=relative_to_assets("entry_7.png"))
entry_bg_7 = canvas.create_image(252.0,538.5,image=entry_image_7)
step_step = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 24 * -1))
step_step.place(x=212.0,y=515.0,width=80.0,height=50.0)

entry_image_8 = PhotoImage(file=relative_to_assets("entry_8.png"))
entry_bg_8 = canvas.create_image(409.0,538.5,image=entry_image_8)
step_end = Entry(bd=0,bg="#D9D9D9",fg="#000716",highlightthickness=0,relief="flat",font=("Starbirl", 24 * -1))
step_end.place(x=367.0,y=515.0,width=80.0,height=50.0)

canvas.create_text(32.0, 476.0, anchor="nw", text="START", fill="#B4A368", font=("Starbirl", 20 * -1) )
canvas.create_text(202.0,475.0,anchor="nw",text="STEP",fill="#B4A368",font=("Starbirl", 20 * -1))
canvas.create_text(161.0,498.0,anchor="nw",text=":",fill="#B4A368",font=("Starbirl", 64 * -1))
canvas.create_text(318.0,498.0,anchor="nw",text=":",fill="#B4A368",font=("Starbirl", 64 * -1))
canvas.create_text(371.0,475.0,anchor="nw",text="END",fill="#B4A368",font=("Starbirl", 20 * -1))

button_image_5 = PhotoImage(file=relative_to_assets("button_5.png"))
step_button = Button(image=button_image_5,borderwidth=0,highlightthickness=0,
                        command=lambda: update_step(step_start.get(),step_step.get(),step_end.get()),relief="flat")
step_button.place(x=598.0,y=510.0,width=213.2259521484375,height=53.45170593261719)


canvas.create_rectangle(20.0,631.0,667.0,674.0,fill="#D9D9D9",outline="#242424")

impulsePImgi = PhotoImage(file=relative_to_assets("entry_3.png"))
impulsePImg = canvas.create_image(55.5,653.0,image=impulsePImgi)
impulseP = Text(bg="#D9D9D9",relief="flat", fg="#000716",highlightthickness=0,font=("Starbirl", 24 * -1))
impulseP.place(x=21,y=639.0,width=71.0,height=30)

impulse_slider = tk.Scale(window, from_=0, to=100, orient=tk.HORIZONTAL, bg="#242424",
                          command=lambda value: update_text(impulseP, value),showvalue=0)
impulse_slider.place(x=100.0,y=641,width=495,height=20)
button_image_6 = PhotoImage(file=relative_to_assets("button_6.png"))
impulse_button = Button(image=button_image_6,borderwidth=0,highlightthickness=0,
                        command=lambda: update_imp(impulse_slider.get()),relief="flat")
impulse_button.place(x=598.0,y=626.0,width=213.2259521484375,height=53.45170593261719)

# canvas.create_rectangle(10.0,163,705.0,168.0,fill="#797979",outline="")
# canvas.create_rectangle(10.0,304,705.0,309.0,fill="#797979",outline="")
# canvas.create_rectangle(6.0,460,701.0,465.0,fill="#797979",outline="")
# canvas.create_rectangle(6.0,611,701.0,616.0,fill="#797979",outline="")

canvas.create_text(50.0,252.0,anchor="nw",text="CONTINUOUS OUTPUT ?",fill="#FFFFFF",font=("Starbirl", 16 * -1))

button_image_7 = PhotoImage(file=relative_to_assets("button_7.png"))
enable_button = Button(image=button_image_7,borderwidth=0,highlightthickness=0,
                       command=lambda: send_command("GOOD"),relief="flat")
enable_button.place(x=1115.0,y=571.0,width=266.0,height=107.0)

button_image_8 = PhotoImage(file=relative_to_assets("button_8.png"))
log_button = Button(image=button_image_8,borderwidth=0,highlightthickness=0,
                    command=lambda: send_command("LOG:TIME"),relief="flat")
log_button.place(x=893.0,y=571.0,width=195.34249877929688,height=105.0)

button_image_9 = PhotoImage(file=relative_to_assets("button_9.png"))
estop_button = Button(image=button_image_9,borderwidth=0,highlightthickness=0,
                      command=lambda: send_command("STOP"),relief="flat")
estop_button.place(x=902.0,y=91.0,width=479.41448974609375,height=160.36000061035156)

image = Image.open(relative_to_assets("gtsc_long logo - invert.png"))
resized_image = image.resize((795, 138), Image.LANCZOS) # Use appropriate resampling filter
photo_image = ImageTk.PhotoImage(resized_image)
canvas.create_image(380,750,image=photo_image)

window.resizable(False, False)

# Start a background thread for reading serial data
serial_thread = threading.Thread(target=read_serial, daemon=True)

# Start the heartbeat thread
heartbeat_thread = threading.Thread(target=send_heartbeat, daemon=True)

com_ports = get_com_ports()
# Check if COM ports are available
if com_ports:
    # Create a label for the combobox
    canvas.create_text(902,16,anchor="nw",text="Select COM Port:",fill="#FFFFFF",font=("Starbirl", 20 * -1))

    # Create the combobox with the list of COM ports
    com_port_combobox = ttk.Combobox(window, values=com_ports, state="readonly",font=("Starbirl", 24 * -1), )
    com_port_combobox.place(x=902,y=40,width=500,height=28)
    
    # Bind the combobox selection to an event
    com_port_combobox.bind("<<ComboboxSelected>>", on_select_com_port)

else:
    canvas.create_text(902,16,anchor="nw",text="No COM ports found",fill="#FFFFFF",font=("Starbirl", 20 * -1))

window.mainloop()
