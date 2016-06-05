import os
import sys

class KlikCompatPython(object):
	# All patches we need to run python applications in a cross distro manner
	
	def __init__(self, klik):
		self.klik = klik
	
	def init_enviroment(self, cmg):

		# PYTHON MODULES FROM DEBIAN RECIPES

		# Initialization of PYTHONPATH
		environ_pythonpath = ":".join(sys.path)
		if os.getenv("PYTHONPATH") != None:
			environ_pythonpath = os.getenv("PYTHONPATH")
		
		# fix python path for unregistered modules... note we arn't byte compiling...!!
		# this makes emma work
		paths = cmg.find_sub_directories("/usr/share/pycentral")
		for path in paths:
			# for each folder add folder/site-packages to python path
			#path = os.path.join(cmg.mount_point_path, path[1:], "site-packages")
			path = os.path.join(path, "site-packages")
			environ_pythonpath = environ_pythonpath + ":" + path 
			
			# check for pth files
			for pth in os.listdir(cmg.mount_point_path + path):
				path = cmg.mount_point_path + path + pth
				if os.path.isfile( path ) and pth.endswith("pth"):
					f = open(path, "r")
					pth = os.path.join(path, f.read().strip())
					f.close()
					environ_pythonpath = environ_pythonpath + ":" + pth

		# this makes istanbul work
		paths = cmg.find_sub_directories("/usr/share/python-support")
		for path in paths:
			# for each folder add folder/site-packages to python path
			environ_pythonpath = environ_pythonpath + ":" + path

			# check for pth files
			for pth in os.listdir(cmg.mount_point_path + path):
				path = cmg.mount_point_path + path + pth
				if os.path.isfile( path ) and pth.endswith("pth"):
					f = open(path, "r")
					pth = os.path.join(path, r.read().strip())
					f.close()
					environ_pythonpath = environ_pythonpath + ":" + os.path.join(path, pth)

		os.environ["PYTHONPATH"] = environ_pythonpath
		print "PYTHONPATH >> %s" % os.environ["PYTHONPATH"]

