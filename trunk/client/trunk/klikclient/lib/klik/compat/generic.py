import os

class KlikCompatGeneric(object):

	def __init__(self, klik):
		self.klik = klik

	def init_enviroment(self, cmg):

		# Update path to work across distros
		os.environ["PATH"] = "/usr/lib/sbin:/bin:/usr/sbin:/usr/bin:/usr/X11R6/bin:/usr/local/sbin:/usr/local/bin:/usr/games:" + os.environ["PATH"]
		if not self.klik.settings.glibc_loader:
			os.environ["LD_LIBRARY_PATH"] = "/usr/lib:%s/lib:%s/lib:%s/usr/lib/:%s/opt/kde3/lib:%s/usr/X11R6/lib/".replace("%s", cmg.mount_point_path)
		else:
			os.environ["LD_LIBRARY_PATH"] = "/usr/lib:/lib:/opt/kde3/lib:/usr/X11R6/lib/"
	
		# USR LIB PATHS
		# add all /usr/lib sub folders to LD_LIBRARY_PATH
		# This fix makes wireshark work, why dosnt wireshark see the union?
		if cmg.cmg_version == 2:
			directories = cmg.find_sub_directories("/usr/lib")
			for dir in directories:
				if not self.klik.settings.glibc_loader:
					os.environ["LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"] + ":" + os.path.join(cmg.mount_point_path, dir[1:])
				else:
					os.environ["LD_LIBRARY_PATH"] = os.environ["LD_LIBRARY_PATH"] + ":" + dir[1:]
	
		return
