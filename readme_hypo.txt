kingpin model importer/exporter for noesis
============
based of the offical md2 SDK.
extended texture search paths. including main\ (when working in mod folders)
-mdxplayer and -md2export switch added to fix player model seams issues
use when model program does not support md2/mdx export


notes: player model exporter option prevents seams between each body part by using a common grid.
    but if your model has its seams hidden then use default exporter. this will lower vertical mesh wobble(caused by compression).



exporting player models
=======================
-mdxexport and -md2export (will export player/weapon models)
in your modelling program the start of mesh names must be set for the exported model(fbx etc..)
mesh name can contain any subsequent characters after these defined names below. eg. "head_eyes" "head_ear"
  head
  body
  legs
  w_bazooka
  w_flame
  w_grenade
  w_heavy
  w_pipe
  w_pistol
  w_shot
  w_tom

export model into main/players/male_xx
make sure head_001.tga, body_001.tga and legs_001.tga are in this folder




version 1.03
============
added player model exporter. use -mdxplayer in exporter dialog box (-md2player works for .md2 output if needed)
  export folder will be used to place the 3 player model files (head/body/legs).mdx and any weapons

import model folder paths will now look for a folder called kingpin if texture not found
  this fixes models stored in a mod\ folder and image is in main\ folder
  search order .tga, .pcx, .jpg, .png, .dds

export will try to look for diffuse images(fbx) to get correct directory for exported model
  texture folder must contain "\players\" or "\models\" or "\textures\" to become valid

all image files found will be inserted into model. max of 32 alowed (only 1 used for player models)
  models can only render 1 texture at a time but code\mods can choose what texture index to render

import model now allocates all images refrenced in model. to assist export.

better implementation of memory use

fixed missing object number


version 1.02
============
mdx exporter implemented

version 1.01
============
fix: when a files is clicked->open with. noesis cant find the image path


version 1.0
============
inial release. only importer working