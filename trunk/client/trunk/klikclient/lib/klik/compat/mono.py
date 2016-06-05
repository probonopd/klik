import os

class KlikCompatMono(object):
	# All patches we need to run mono applications in a cross distro manner

	def __init__(self, klik):
		self.klik = klik

	# what happend to our old mono wapi path hack?
	# I think we removed it as didnt need it any more...
	def init_enviroment(self, cmg):
		return
