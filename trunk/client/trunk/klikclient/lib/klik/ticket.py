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


import sys
import os
from xml.dom.minidom import *
from urlparse import urlparse

		
class KlikTicket (object):

	def __init__(self, path=None):
		self.name = ""
		
		if not path == None:
			self.load_from_file(path)
 
	def load_from_file(self, path):
		try:
			f = open(path, "r")
			xml = f.read()
			f.close()
			self.load_from_string(xml)
		except Exception, e:
			raise SafeException("Error recipe could not be loaded does not appear to be valid XML (%s)" % e)	

	def load_from_string(self, xml):
		try:
			self.original_xml = xml
			document = parseString(xml)
			
			
		except Exception, e:
			raise SafeException("Error recipe could not be loaded does not appear to be valid XML (%s)" % e)

		self.__load_from_document(document)
		

	def __load_from_document(self, document):
	
		try:
			interface = self.__getFirstNode(document,"ticket")
			self.name = self.__getNodeValue(interface, None)
		
		except:
			raise Exception("XML document does not appear to be a valid ticket")
		

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



