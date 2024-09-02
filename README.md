# Behave AI

An AI toolkit for O3DE.

Behave is a package used as a starting point to implement AI capabilites in O3DE projects. Currently, it provides two main features:

- A BehaviorTree engine, built on top of [BehaviorTree.CPP](https://github.com/BehaviorTree/BehaviorTree.CPP), with a dedicated editor (see capture below)
- [WIP] A 2D Navigation and Path Finding, built on top of [Recast Navigation](https://github.com/recastnavigation/recastnavigation).

### Work in progress

- https://github.com/o3de/o3de/pull/14415 removed some needed dependencies which will need to be re-added via this gem
- We should target the o3de recast gem and not have a copy in this gem
- BehaviorTree.CPP needs to be built by hand outside of o3de which is annoying, need to fix that
- [QtNodeEditor](https://github.com/paceholder/nodeeditor) also need to be built by hand using an old version of QT and QTCreator software. Either need to remove this deps (given that o3de already provide a nodding GUI) or remove these builds steps

![Doc](doc/behaviortree.png)
