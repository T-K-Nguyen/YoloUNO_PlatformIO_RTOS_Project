Import("env")

def after_upload(source, target, env):
    print(">>> Uploading LittleFs data folder...")
    env.Execute("pio run --target uploadfs")

env.AddPostAction("upload", after_upload)