import subprocess
from datetime import datetime
import os

def generate_version_file(
    file_name="version.txt",
    version=(1, 0, 0, 0),
    company_name="Georgia Tech Supersonics",
    file_description="Computerized Remote Control System Interface",
    internal_name="CRCS",
    original_filename="CRCS.exe",
    product_name="Computerized Remote Control System"
):
    file_version = ".".join(map(str, version))
    now = datetime.now()

    content = f"""# UTF-8
VSVersionInfo(
  ffi=FixedFileInfo(
    filevers={version},
    prodvers={version},
    mask=0x3f,
    flags=0x0,
    OS=0x40004,
    fileType=0x1,
    subtype=0x0,
    date=({now.year}, {now.month})
    ),
  kids=[
    StringFileInfo([
      StringTable(
        '040904B0',
        [StringStruct('CompanyName', '{company_name}'),
         StringStruct('FileDescription', '{file_description}'),
         StringStruct('FileVersion', '{file_version}'),
         StringStruct('InternalName', '{internal_name}'),
         StringStruct('OriginalFilename', '{original_filename}'),
         StringStruct('ProductName', '{product_name}'),
         StringStruct('ProductVersion', '{file_version}')])
      ]),
    VarFileInfo([VarStruct('Translation', [1033, 1200])])
  ]
)
"""
    with open(file_name, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"[+] Version info file written to '{file_name}'")

def build_gui(script_name="./gui.py", icon=".\\assets\\frame0\\gtsc_icon - invert.png"):
    generate_version_file()
    
    pyinstaller_cmd = [
        "pyinstaller",
        "--onefile",
        "--windowed",
        "--name", "CRCS",
        "--add-data", "assets/;assets/frame0",
        "--icon", icon,
        "--version-file", "version.txt",
        script_name
    ]

    print(f"[+] Running PyInstaller...\n{' '.join(pyinstaller_cmd)}")
    subprocess.run(pyinstaller_cmd)

if __name__ == "__main__":
    build_gui()
