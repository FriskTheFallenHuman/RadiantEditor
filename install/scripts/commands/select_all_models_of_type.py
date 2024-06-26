# Set the command name so that RadiantEditor recognises this file
__commandName__ = 'SelectAllModelsOfType'
__commandDisplayName__ = 'Select all Models of same type'

# The actual algorithm called by RadiantEditor is contained in the execute()
# function
def execute():
	# Collect all currently selected models
	selectedModelNames = {}

	import radiant as dr

	class Walker(dr.SelectionVisitor) :
		def visit(self, node):
			# Try to "cast" the node to an entity
			entity = node.getEntity()

			if not entity.isNull():
				if not entity.getKeyValue('model') == '':
					selectedModelNames[entity.getKeyValue('model')] = 1

	visitor = Walker()
	GlobalSelectionSystem.foreachSelected(visitor)

	print('Unique models currently selected: ' + str(len(selectedModelNames)))

	# Now traverse the scenegraph, selecting all of the same names
	class SceneWalker(dr.SceneNodeVisitor) :
		def pre(self, node):

			# Try to "cast" the node to an entity
			entity = node.getEntity()

			if not entity.isNull():
				modelName = entity.getKeyValue('model')

				if not modelName == '' and modelName in selectedModelNames:
					# match, select this node
					node.setSelected(1)

				# SteveL #3690: fixing return values
				return 0 # don't traverse this entity's children
			return 1 # not an entity, so traverse children

	walker = SceneWalker()
	GlobalSceneGraph.root().traverse(walker)

# __executeCommand__ evaluates to true after RadiantEditor has successfully
# initialised
if __executeCommand__:
	execute()
