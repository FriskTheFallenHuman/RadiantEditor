# Set the command name so that RadiantEditor recognises this file
__commandName__ = 'ShiftTexturesUpwardsRandomly'
__commandDisplayName__ = 'Shift Textures randomly upwards'

# The actual algorithm called by RadiantEditor
# is contained in the execute() function

def execute():
	import random
	import radiant as dr

	class FaceVisitor(dr.SelectedFaceVisitor) :
		def visitFace(self, face):
			t = random.randint(0, 100) / 100
			face.shiftTexdef(0, t)

	visitor = FaceVisitor()
	GlobalSelectionSystem.foreachSelectedFace(visitor)

	GlobalCameraManager.getActiveView().refresh()

# The variable __executeCommand__ evaluates to true
# when RadiantEditor executes this command
if __executeCommand__:
	execute()
