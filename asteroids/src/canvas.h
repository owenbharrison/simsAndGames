//compiler gets mad when i include sokol_gfx...
#pragma once
#ifndef CANVAS_STRUCT_H
#define CANVAS_STRUCT_H

//render target
struct Canvas {
	int width=0, height=0;

	sg_image color_img{SG_INVALID_ID};
	sg_view color_attach{SG_INVALID_ID};
	sg_view color_tex{SG_INVALID_ID};

	sg_image depth_img{SG_INVALID_ID};
	sg_view depth_attach{SG_INVALID_ID};

	//this needs to free & remake resources
	void resize(int w, int h) {
		//avoid unnecessary reallocations
		if(w==width&&h==height) return;

		width=w;
		height=h;

		//remake color img
		sg_destroy_image(color_img);
		sg_image_desc col_img_desc{};
		col_img_desc.usage.color_attachment=true;
		col_img_desc.width=width;
		col_img_desc.height=height;
		color_img=sg_make_image(col_img_desc);

		//remake color attach
		sg_destroy_view(color_attach);
		sg_view_desc col_attach_desc{};
		col_attach_desc.color_attachment.image=color_img;
		color_attach=sg_make_view(col_attach_desc);

		//remake color tex
		sg_destroy_view(color_tex);
		sg_view_desc col_tex_desc{};
		col_tex_desc.texture.image=color_img;
		color_tex=sg_make_view(col_tex_desc);

		//remake depth img
		sg_destroy_image(depth_img);
		sg_image_desc depth_img_desc{};
		depth_img_desc.usage.depth_stencil_attachment=true;
		depth_img_desc.width=width;
		depth_img_desc.height=height;
		depth_img_desc.pixel_format=SG_PIXELFORMAT_DEPTH_STENCIL;
		depth_img=sg_make_image(depth_img_desc);

		//remake depth attach
		sg_destroy_view(depth_attach);
		sg_view_desc depth_attach_desc{};
		depth_attach_desc.depth_stencil_attachment.image=depth_img;
		depth_attach=sg_make_view(depth_attach_desc);
	}
};
#endif