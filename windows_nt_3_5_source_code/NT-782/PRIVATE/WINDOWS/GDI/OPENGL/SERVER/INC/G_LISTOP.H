/*
** Copyright 1991-1993, Silicon Graphics, Inc.
** All Rights Reserved.
** 
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
** 
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
*/


/* AUTOMATICALLY UPDATED OR GENERATED BY SGI: DO NOT EDIT */

#ifndef _g_listop_h_
#define _g_listop_h_

#define __glop_CallList                  0
#define __glop_CallLists                 1
#define __glop_ListBase                  2
#define __glop_Begin                     3
#define __glop_Bitmap                    4
#define __glop_Color3bv                  5
#define __glop_Color3dv                  6
#define __glop_Color3fv                  7
#define __glop_Color3iv                  8
#define __glop_Color3sv                  9
#define __glop_Color3ubv                10
#define __glop_Color3uiv                11
#define __glop_Color3usv                12
#define __glop_Color4bv                 13
#define __glop_Color4dv                 14
#define __glop_Color4fv                 15
#define __glop_Color4iv                 16
#define __glop_Color4sv                 17
#define __glop_Color4ubv                18
#define __glop_Color4uiv                19
#define __glop_Color4usv                20
#define __glop_EdgeFlagv                21
#define __glop_End                      22
#define __glop_Indexdv                  23
#define __glop_Indexfv                  24
#define __glop_Indexiv                  25
#define __glop_Indexsv                  26
#define __glop_Normal3bv                27
#define __glop_Normal3dv                28
#define __glop_Normal3fv                29
#define __glop_Normal3iv                30
#define __glop_Normal3sv                31
#define __glop_RasterPos2dv             32
#define __glop_RasterPos2fv             33
#define __glop_RasterPos2iv             34
#define __glop_RasterPos2sv             35
#define __glop_RasterPos3dv             36
#define __glop_RasterPos3fv             37
#define __glop_RasterPos3iv             38
#define __glop_RasterPos3sv             39
#define __glop_RasterPos4dv             40
#define __glop_RasterPos4fv             41
#define __glop_RasterPos4iv             42
#define __glop_RasterPos4sv             43
#define __glop_Rectdv                   44
#define __glop_Rectfv                   45
#define __glop_Rectiv                   46
#define __glop_Rectsv                   47
#define __glop_TexCoord1dv              48
#define __glop_TexCoord1fv              49
#define __glop_TexCoord1iv              50
#define __glop_TexCoord1sv              51
#define __glop_TexCoord2dv              52
#define __glop_TexCoord2fv              53
#define __glop_TexCoord2iv              54
#define __glop_TexCoord2sv              55
#define __glop_TexCoord3dv              56
#define __glop_TexCoord3fv              57
#define __glop_TexCoord3iv              58
#define __glop_TexCoord3sv              59
#define __glop_TexCoord4dv              60
#define __glop_TexCoord4fv              61
#define __glop_TexCoord4iv              62
#define __glop_TexCoord4sv              63
#define __glop_Vertex2dv                64
#define __glop_Vertex2fv                65
#define __glop_Vertex2iv                66
#define __glop_Vertex2sv                67
#define __glop_Vertex3dv                68
#define __glop_Vertex3fv                69
#define __glop_Vertex3iv                70
#define __glop_Vertex3sv                71
#define __glop_Vertex4dv                72
#define __glop_Vertex4fv                73
#define __glop_Vertex4iv                74
#define __glop_Vertex4sv                75
#define __glop_ClipPlane                76
#define __glop_ColorMaterial            77
#define __glop_CullFace                 78
#define __glop_Fogf                     79
#define __glop_Fogfv                    80
#define __glop_Fogi                     81
#define __glop_Fogiv                    82
#define __glop_FrontFace                83
#define __glop_Hint                     84
#define __glop_Lightf                   85
#define __glop_Lightfv                  86
#define __glop_Lighti                   87
#define __glop_Lightiv                  88
#define __glop_LightModelf              89
#define __glop_LightModelfv             90
#define __glop_LightModeli              91
#define __glop_LightModeliv             92
#define __glop_LineStipple              93
#define __glop_LineWidth                94
#define __glop_Materialf                95
#define __glop_Materialfv               96
#define __glop_Materiali                97
#define __glop_Materialiv               98
#define __glop_PointSize                99
#define __glop_PolygonMode             100
#define __glop_PolygonStipple          101
#define __glop_Scissor                 102
#define __glop_ShadeModel              103
#define __glop_TexParameterf           104
#define __glop_TexParameterfv          105
#define __glop_TexParameteri           106
#define __glop_TexParameteriv          107
#define __glop_TexImage1D              108
#define __glop_TexImage2D              109
#define __glop_TexEnvf                 110
#define __glop_TexEnvfv                111
#define __glop_TexEnvi                 112
#define __glop_TexEnviv                113
#define __glop_TexGend                 114
#define __glop_TexGendv                115
#define __glop_TexGenf                 116
#define __glop_TexGenfv                117
#define __glop_TexGeni                 118
#define __glop_TexGeniv                119
#define __glop_InitNames               120
#define __glop_LoadName                121
#define __glop_PassThrough             122
#define __glop_PopName                 123
#define __glop_PushName                124
#define __glop_DrawBuffer              125
#define __glop_Clear                   126
#define __glop_ClearAccum              127
#define __glop_ClearIndex              128
#define __glop_ClearColor              129
#define __glop_ClearStencil            130
#define __glop_ClearDepth              131
#define __glop_StencilMask             132
#define __glop_ColorMask               133
#define __glop_DepthMask               134
#define __glop_IndexMask               135
#define __glop_Accum                   136
#define __glop_Disable                 137
#define __glop_Enable                  138
#define __glop_PopAttrib               139
#define __glop_PushAttrib              140
#define __glop_Map1d                   141
#define __glop_Map1f                   142
#define __glop_Map2d                   143
#define __glop_Map2f                   144
#define __glop_MapGrid1d               145
#define __glop_MapGrid1f               146
#define __glop_MapGrid2d               147
#define __glop_MapGrid2f               148
#define __glop_EvalCoord1dv            149
#define __glop_EvalCoord1fv            150
#define __glop_EvalCoord2dv            151
#define __glop_EvalCoord2fv            152
#define __glop_EvalMesh1               153
#define __glop_EvalPoint1              154
#define __glop_EvalMesh2               155
#define __glop_EvalPoint2              156
#define __glop_AlphaFunc               157
#define __glop_BlendFunc               158
#define __glop_LogicOp                 159
#define __glop_StencilFunc             160
#define __glop_StencilOp               161
#define __glop_DepthFunc               162
#define __glop_PixelZoom               163
#define __glop_PixelTransferf          164
#define __glop_PixelTransferi          165
#define __glop_PixelMapfv              166
#define __glop_PixelMapuiv             167
#define __glop_PixelMapusv             168
#define __glop_ReadBuffer              169
#define __glop_CopyPixels              170
#define __glop_DrawPixels              171
#define __glop_DepthRange              172
#define __glop_Frustum                 173
#define __glop_LoadIdentity            174
#define __glop_LoadMatrixf             175
#define __glop_LoadMatrixd             176
#define __glop_MatrixMode              177
#define __glop_MultMatrixf             178
#define __glop_MultMatrixd             179
#define __glop_Ortho                   180
#define __glop_PopMatrix               181
#define __glop_PushMatrix              182
#define __glop_Rotated                 183
#define __glop_Rotatef                 184
#define __glop_Scaled                  185
#define __glop_Scalef                  186
#define __glop_Translated              187
#define __glop_Translatef              188
#define __glop_Viewport                189
#define __GL_NUM_LISTABLE_CMDS 190


#endif  /* _g_listop_h_ */
