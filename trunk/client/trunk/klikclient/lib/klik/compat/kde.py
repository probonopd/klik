import os
from subprocess import *

class KlikCompatKDE(object):

	def __init__(self, klik):
		self.klik = klik

	def init_enviroment(self, cmg):

		if "KDE" in cmg.recipe.needs:

			# used for the sandbox feature
			os.environ["KDE_FORK_SLAVES"] = "1"

			# if starting a kde app make sure kdeinit is running first
			if self.klik.xdg.get_desktop_enviroment != "KDE":

				p = Popen(["/usr/bin/pgrep", "kdeinit"], stdout=PIPE, stdin=PIPE, stderr=PIPE)
				p.wait()
				if p.returncode != 0:
					call(["kdeinit"])
		return
