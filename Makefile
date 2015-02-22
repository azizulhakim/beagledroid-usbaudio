beagleaudio-y :=  aoa.o adk.o beagle-audio.o

obj-m += beagleaudio.o

all:
	make -w -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm *~
