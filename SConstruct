env = Environment()
env.Library("usbhost/usbhost.c")
env.Program("accessorychat.c",
        LIBS=["usbhost", "pthread"],
        LIBPATH='usbhost',
        CPPPATH = ['.', 'usbhost'])
