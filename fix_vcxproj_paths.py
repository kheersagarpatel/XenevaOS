import os
import glob

def fix_paths(filepath):
    with open(filepath, 'r') as f:
        content = f.read()

    # Replacements
    replacements = [
        ("E:\\Xeneva Project\\Aurora\\BaseHdr", "$(SolutionDir)BaseHdr"),
        ("E:\\Xeneva Project\\Aurora\\Libs", "$(SolutionDir)Libs"),
        ("E:\\Xeneva Project\\Aurora\\x64\\Debug", "$(SolutionDir)Build"),
        ("E:\\Xeneva Project\\Aurora", "$(SolutionDir)"),
        ("..\\..\\x64\\Debug", "$(SolutionDir)Build"),
    ]

    new_content = content
    for old, new in replacements:
        new_content = new_content.replace(old, new)

    if new_content != content:
        with open(filepath, 'w') as f:
            f.write(new_content)
        print(f"Fixed {filepath}")

vcxproj_files = glob.glob('**/*.vcxproj', recursive=True)
for vcxproj in vcxproj_files:
    fix_paths(vcxproj)

