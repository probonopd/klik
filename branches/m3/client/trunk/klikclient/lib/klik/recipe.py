#!/usr/bin/env python
# -*- coding: utf-8 -*-

#   klik2 
#   Copyright (C) 2008  Jason Taylor - killerkiwi2005@gmail.com
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

# http://0install.net/interface-spec.html

import sys
import os
import shutil
from xml.dom.minidom import *
from urlparse import urlparse
from klik.zeroinstallgpg.gpg import *
from klik.exception import SafeException

class Icon (object):
	size = None
	src = None
	file_type = None
	
class Symlink (object):
	pathFrom = None
	pathTo = None	


class Enviroment (object):
	name = None
	insert = None
	mode = 'prepend' #'prepend|append|replace' ?
	default = ''

class KlikIngredientPackage(object):
	href = None
	md5 = None
	name = None
	
	def __init__(self, href, name = None, md5 = None):
		self.href = href
		self.name = name
		self.md5 = md5

class KlikTustredAction (object):

	def clean_path(self, base_path, path):
		if path[0] == "/":
			path = path[1:]
		p = os.path.join( base_path, path)
		p = os.path.expanduser( p )
		p = os.path.expandvars( p )
		p = os.path.abspath( p )

		if p.startswith( base_path ):
			return p
		return None
		

class KlikTustredActionUnzip(KlikTustredAction):
	def __init__(self, fileName):
		self.fileName = fileName
	
	def get_command(self, archivepath):
	

	
		fn = self.clean_path(archivepath, self.fileName)
		
		
	    
		if fn.lower().endswith(".tgz") or fn.lower().endswith(".tar.gz"):
			return ["tar", "zxvf", fn]
			
		if fn.lower().endswith(".tbz") or fn.lower().endswith(".tar.bz2") or fn.lower().endswith(".tbz2"):
			return ["bunzip2", "-c", fn]
		
		if fn.lower().endswith(".zip"):
			return ["unzip", fn]
		
		return None


class KlikTustredActionMove(KlikTustredAction):
	def __init__(self, source, destination):
		self.source = source
		self.destination = destination
	
	def get_command(self, basepath):
		pf = self.clean_path( basepath, self.source )
		pt = self.clean_path( basepath, self.destination )
		if pf != None and pt != None and os.path.exists(pf):
			return ["mv", pf, pt]
		return None
		
class KlikTustredActionDelete(KlikTustredAction):
	def __init__(self, path):
		self.path = path
		
	def get_command(self, basepath):
		p = self.clean_path( basepath, self.path)
		if p != None and os.path.exists(p):
			return ["rm", p]
		return None
		
class KlikTustredActionSymLink(KlikTustredAction):
	def __init__(self, source, destination):
		self.source = source
		self.destination = destination
	def get_command(self, basepath):
		pf = self.clean_path( basepath, self.source )
		pt = self.clean_path( basepath, self.destination )
		if pf != None:
			return ["ln", "-sf", self.destination, pf]
		return None
		
class KlikTustredActionInformation(KlikTustredAction):
	def __init__(self, text):
		self.text = text
		
class KlikScript(object):
	def __init__(self, script):
		self.script = script		

class KlikTustredActionCollection(object):
	def __init__(self):
		self.move = []
		self.delete = []
		self.symlink = []
		self.information = []
		self.script = []
		self.unzip = []
		
class KlikRecipe (object):

	def __init__(self):
		self.name = ""
		self.version = 0
		self.source = None
		self.maintainer = ""
		self.creationdate = None
		self.accept_merge = False
		self.system = "LSB3"
		self.desktop = None
		self.description = ""
		self.summary = ""
		self.command = None
		
		self.icon = None
		self.packages = []

		self.original_xml = None
		self.size = None
		self.source_uri = None
		
		self.preflight = []
		self.postflight = []
		self.prerun = []
		self.postrun = []

		self.require_terminal = False

		self.debtags = { }
		
		self.enviroment = []

	def save(self, path):
		print "TODO: create recipie"

 	def is_trusted(self):

 		flag = False
 		signed_by = "UNTRUSTED : Recipe could not be verified"
		try:
		 	stream, sigs = check_string( self.original_xml )
		 	assert sigs
		 	
		 	for sig in sigs:
		 		signed_by = str(sig)
		 		if isinstance(sig, ValidSig) == True :
		 			signed_by = sig.get_details()[1][9] # get the name of the person who signed 
		 			flag = True
		 			break
		except:
			pass

 		return [flag, signed_by]
 
 
	def load_from_file(self, path):
		try:
			f = open(path, "r")
			xml = f.read()
			f.close()
			self.load_from_string(xml)
		except:
			raise SafeException("Error recipe could not be loaded does not appear to be valid XML")	
		

	def load_from_string(self, xml):
		try:
			self.original_xml = xml
			document = parseString(xml)
			
		except:
			raise SafeException("Error recipe could not be loaded does not appear to be valid XML")
				
				
		self.__load_from_document(document)
		
			
	def __load_from_document(self, document):
	
		interface = self.__getFirstNode(document,"interface")
		error = self.__getFirstNode(document,"error")
			
		if interface != None:
			self.source_uri = interface.getAttribute("uri")
			
			name = self.__getFirstNode(interface, "name")
			
			if name != None and name.firstChild != None:
				self.name = self.__getNodeValue(name, "")
				
				# default command is app name
				self.command = self.name
				
				description = self.__getFirstNode(interface,"description")
				if description != None:
					self.description = self.__getNodeValue( description, "")
					
				summary = self.__getFirstNode(interface,"summary")
				if summary != None:
					self.summary = self.__getNodeValue( summary, "")
				
				icon = self.__getFirstNode(interface,"icon")
				if icon != None:
					self.icon = icon.getAttribute("href")

				debtags = self.__getFirstNode(interface,"debtags")
				if debtags != None:
					nodes = interface.getElementsByTagName("tag")
					
					if len(nodes) > 0:
						self.require_terminal = False
					for node in nodes:
						value = self.__getNodeValue( node, None )
						if value != None:
							if value == "interface::commandline":
								self.require_terminal = True
							value = value.split("::")
							self.debtags[value[0]] = value[1]

				
				group = self.__getFirstNode(interface,"group")				
				if group != None:
					if group.attributes["main"] != None:
						self.command = group.getAttribute("main")

					implementation = self.__getFirstNode(group,"implementation")		
						
					if implementation != None:
						self.version = implementation.getAttribute("version")
						if implementation.attributes["downloadsize"] != None:
							try:
								self.size = implementation.getAttribute("downloadsize")
							except:
								pass
						
					for node in implementation.getElementsByTagName("environment"):
						try:
							__readEnviroment( node )   	  	
						except:
							pass
					
					
					for node in implementation.getElementsByTagName("archive"):
						try:
							ki = KlikIngredientPackage(node.getAttribute( "href" ), node.getAttribute( "name" ), node.getAttribute( "md5" ))
							self.packages.append( ki )   	  			
						except:
							pass
							
							
					preflight = self.__getFirstNode(implementation, "klik-preflight")	
					self.__readActions( preflight, self.preflight)
					
					postflight = self.__getFirstNode(implementation, "klik-postflight")	
					self.__readActions( postflight, self.postflight)
					
					prerun = self.__getFirstNode(implementation, "klik-prerun")	
					self.__readActions( prerun, self.prerun)
					
					postrun = self.__getFirstNode(implementation, "klik-postrun")	
					self.__readActions( postrun, self.postrun)

				return 
		elif error != None:
			# hack to trim quotes... why does this happen?
			raise SafeException( self.__getNodeValue( error, "") )
			return
		
		raise Exception("XML document does not appear to be a valid recipe")	
		
		
	def __readEnviroment( self, node ):
		name = node.getAttribute( "name" )
		
		if name != None and name != "":
			env = Enviroment()
			env.name = name
			env.insert = node.getAttribute( "insert" )
			env.mode = node.getAttribute( "mode" )
			env.default = node.getAttribute( "default" )
			
			self.enviroment.append(env) 

	def __readActions(self, parent_node, action_collection):

		if parent_node != None:
			
			for node in parent_node.childNodes:
				
				if node.nodeType == 1:
					if node.tagName == "move":
						source = node.getAttribute("source")
						destination = node.getAttribute("destination")
						if source != None and destination != None:
							action_collection.append( KlikTustredActionMove(source, destination) )
				
					if node.tagName == "delete":
						path = node.getAttribute("path")
						if path != None:
							action_collection.append( KlikTustredActionDelete(path) )
			
					if node.tagName == "symlink":
						source = node.getAttribute("source")
						destination = node.getAttribute("destination")
						if source != None and destination != None:
							action_collection.append( KlikTustredActionSymLink(source, destination) )
			
					if node.tagName == "information":
						text = self. __getNodeValue( node, "" )
						if text != "":
							action_collection.append( KlikTustredActionInformation(text) )
							
					if node.tagName == "script":
						content = self. __getNodeValue( node, "" )
						if content != "":
							action_collection.append( KlikScript(content) )

					if node.tagName == "unzip":
						content = self. __getNodeValue( node, "" )
						if content != "":
							action_collection.append( KlikTustredActionUnzip(content) )

	def __getFirstNode(self, element, tagname):
		if element != None:
			nodes =  element.getElementsByTagName(tagname)
			if len(nodes) > 0:
				return nodes[0]
		return None
			
	def __getNodeValue(self, element, defaultValue):
		node =  element.firstChild
		if node != None:
			if node.nextSibling:
				return node.nextSibling.nodeValue
			else:
				return node.nodeValue
		return defaultValue	



