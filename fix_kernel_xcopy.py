import re

with open('Kernel/Kernel.vcxproj', 'r') as f:
    content = f.read()

new_content = re.sub(r'<Command>xcopy.*?F:\\.*?</Command>', '', content)

if new_content != content:
    with open('Kernel/Kernel.vcxproj', 'w') as f:
        f.write(new_content)
    print("Fixed Kernel.vcxproj xcopy")
else:
    print("No xcopy to F: found in Kernel.vcxproj")
