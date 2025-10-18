# ToySurfaceExample
<p align="center">
  <img src="ToySurface_Demo.gif">
</p>

This is a simple example of an MPxSurfaceShape using Maya's C++ API 2024 and VP2.0 that demonstrates how to:

- Create a custom shape from an MPxSurfaceShape.

- Visualize the shape in VP 2.0 using an MPxGeometryOverride.

- Define a component to access the vertices of the custom shape.

- Implement selection of the vertices, either by using the 'select' command in MEL or using the mouse in the viewport.

In this example, the surface is created via an MPxCommand, called Create_Toy_Surface, where the vertices and face indices of the surface are hardcoded values. The code is based on the apiMesh example provided by Autodesk Maya but with a lot less clutter.
