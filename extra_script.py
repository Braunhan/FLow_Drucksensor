from SCons.Script import Import
Import("env")

def after_upload_action(source, target, env):
    print("Starte Upload des Filesystem-Images (SPIFFS)...")
    env.Execute("pio run --target uploadfs")

env.AddPostAction("upload", after_upload_action)
