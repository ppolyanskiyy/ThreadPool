import os

if os.path.isdir("build/"):
   os.system("rmdir /s /q build")

os.system("mkdir build")
os.system("cd build")
os.chdir("build/")
os.system("cmake ../")
os.system("cmake --build . --clean-first")


