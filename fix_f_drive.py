import glob
import re

vcxproj_files = glob.glob('**/*.vcxproj', recursive=True)
for filepath in vcxproj_files:
    with open(filepath, 'r') as f:
        content = f.read()
    
    new_content = re.sub(r'\s*<OutputFile>F:[^<]+</OutputFile>', r'', content)
    
    if new_content != content:
        with open(filepath, 'w') as f:
            f.write(new_content)
        print(f"Fixed {filepath}")

