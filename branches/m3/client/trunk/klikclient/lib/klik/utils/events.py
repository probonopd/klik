#!/usr/bin/env python
# -*- coding: utf-8 -*-

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


# Make C# / Java type evetns
# http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/410686

class Events (object):
   def __getattr__(self, name):
      if hasattr(self.__class__, '__events__'):
         assert name in self.__class__.__events__, \
                "Event '%s' is not declared" % name
      self.__dict__[name] = ev = _EventSlot(name)
      return ev
   def __repr__(self): return 'Events' + str(list(self))
   __str__ = __repr__
   def __len__(self): return NotImplemented
   def __iter__(self):
      def gen(dictitems=self.__dict__.items()):
         for attr, val in dictitems:
            if isinstance(val, _EventSlot):
               yield val
      return gen()

class _EventSlot (object):
   def __init__(self, name):
      self.targets = []
      self.__name__ = name
   def __repr__(self):
      return 'event ' + self.__name__
   def __call__(self, *a, **kw):
      for f in self.targets: f(*a, **kw)
   def __iadd__(self, f):
      self.targets.append(f)
      return self
   def __isub__(self, f):
      while f in self.targets: self.targets.remove(f)
      return self
      
      
      
      
      
######################################################################
## 
## Demo
## 
######################################################################

if __name__ == '__main__':

   class MyEvents(Events):
      __events__ = ('OnChange', )

   class ValueModel(object):
      def __init__(self):
         self.events = MyEvents()
         self.__value = None
      def __set(self, value):
         if (self.__value == value): return
         self.__value = value
         self.events.OnChange()
         ##self.events.OnChange2() # would fail
      def __get(self):
         return self.__value
      Value = property(__get, __set, None, 'The actual value')

   class SillyView(object):
      def __init__(self, model):
         self.model = model
         model.events.OnChange += self.DisplayValue
         ##model.events.OnChange2 += self.DisplayValue # would raise exeception
      def DisplayValue(self):
         print self.model.Value


   model = ValueModel()
   view = SillyView(model)

   print '\n--- Events Demo ---'
   # Events in action
   for i in range(5):
      model.Value = 2*i + 1
   # Events introspection
   print model.events
   for event in model.events:
      print event      
