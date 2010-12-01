/*
 * Toonloop
 *
 * Copyright 2010 Alexandre Quessy
 * <alexandre@quessy.net>
 * http://www.toonloop.com
 *
 * Toonloop is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Toonloop is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the gnu general public license
 * along with Toonloop.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <clutter/clutter.h>
#include "application.h"
#include "clip.h"
#include "gui.h"
#include "infowindow.h"
#include "pipeline.h"
#include "unused.h"
#include "controller.h"

static const float EACH_CLIP_ACTOR_WIDTH = 80.0;

/**
 * Window to display some information. 
 */
InfoWindow::InfoWindow(Application *app) : 
    app_(app),
    stage_(NULL),
    text_(NULL)
{
}

void InfoWindow::on_window_destroyed(ClutterActor &stage, gpointer data)
{
    UNUSED(stage);
    InfoWindow *self = static_cast<InfoWindow *>(data);
    std::cout << "Info window has been deleted" << std::endl;
    self->app_->quit();
}

void InfoWindow::create()
{
    stage_ = clutter_stage_new();
    if (! stage_)
    {
        g_critical("Could not get a stage. The Clutter backend might not support multiple stages.");
    }
    else
    {
        ClutterColor black = { 0x00, 0x00, 0x00, 0xff };
        ClutterColor white = { 0xff, 0xff, 0xff, 0xff };
        ClutterColor gray = { 0x99, 0x99, 0x99, 0xff };
        clutter_stage_set_color(CLUTTER_STAGE(stage_), &black);
        clutter_stage_set_title(CLUTTER_STAGE(stage_), "Toonloop Information");
        
        gfloat width = 640.0f;
        gfloat height = 480.0f;
        clutter_actor_set_size(stage_, width, height);

        // TEXT ABOUT EVERYTHING
        ClutterActor *text_group = clutter_group_new();
        text_ = clutter_text_new_full("Sans semibold 12px", "", &white);
        clutter_container_add_actor(CLUTTER_CONTAINER(text_group), text_);
        clutter_container_add_actor(CLUTTER_CONTAINER(stage_), text_group);
        
        // each image will be 80x60.
        // Plus some text under it

        // Create the layout manager first
        ClutterLayoutManager *layout = clutter_box_layout_new (); // FIXME: memleak?
        //clutter_box_layout_set_homogeneous (CLUTTER_BOX_LAYOUT (layout), TRUE);
        clutter_box_layout_set_spacing (CLUTTER_BOX_LAYOUT (layout), 4);
        // Then create the ClutterBox actor. The Box will take ownership of the ClutterLayoutManager instance by sinking its floating reference
        clipping_group_ = clutter_group_new(); // FIXME: memleak?
        clutter_actor_set_size(clipping_group_, 620.0, 120.0);
        clutter_actor_set_position(clipping_group_, 0.0, 120.0);
        //clutter_actor_set_clip_to_allocation(clipping_group_, TRUE);
        scrollable_box_ = clutter_box_new(layout);
        clutter_container_add_actor(CLUTTER_CONTAINER(clipping_group_), scrollable_box_);
        clutter_container_add_actor(CLUTTER_CONTAINER(stage_), clipping_group_);

        // Add the stuff for the clips:
        // TODO: stop using the MAX_CLIPS constant
        for (unsigned int i = 0; i < MAX_CLIPS; i++)
        {
            //Clip *clip = app_->get_clip(i);
            ClipInfoBox *clip_info_box = new ClipInfoBox;
            clips_[i] = std::tr1::shared_ptr<ClipInfoBox>(clip_info_box);
            clip_info_box->group_ = clutter_group_new();
            clutter_actor_set_size(clip_info_box->group_, EACH_CLIP_ACTOR_WIDTH, 100);

            std::ostringstream os;
            os << "Clip #" << i;
            clip_info_box->image_ = clutter_rectangle_new_with_color(&gray);
            clutter_actor_set_size(clip_info_box->image_, 80, 60);
            clutter_container_add_actor(CLUTTER_CONTAINER(clip_info_box->group_), clip_info_box->image_);

            clip_info_box->label_ = clutter_text_new_full("Sans semibold 8px", os.str().c_str(), &white);
            clutter_actor_set_position(clip_info_box->label_, 2.0, 2.0);
            clutter_container_add_actor(CLUTTER_CONTAINER(clip_info_box->group_), clip_info_box->label_);
            clutter_actor_set_position(clip_info_box->label_, 10, 16);

            clutter_box_pack (CLUTTER_BOX (scrollable_box_), clip_info_box->group_,
                           "x-align", CLUTTER_BOX_ALIGNMENT_END,
                           "expand", TRUE,
                           NULL);
        }

        g_signal_connect(CLUTTER_STAGE(stage_), "delete-event", G_CALLBACK(InfoWindow::on_window_destroyed), this);

        Controller *controller = app_->get_controller();
        controller->choose_clip_signal_.connect(boost::bind( &InfoWindow::on_choose_clip, this, _1));

        clutter_actor_show(stage_);
    }
}

/** Slot for Controller::choose_clip_signal_
 * */
void InfoWindow::on_choose_clip(unsigned int clip_number)
{
    std::cout << __FUNCTION__ << clip_number << std::endl;
}

void InfoWindow::update_info_window()
{
    if (! text_)
        return;
    Gui *gui = app_->get_gui();
    Clip* current_clip = app_->get_current_clip();
    Gui::layout_number current_layout = gui->get_layout();
    Gui::BlendingMode blending_mode = gui->get_blending_mode();
    std::ostringstream os;

    os << "Layout: " << current_layout << " (" << gui->get_layout_name(current_layout) << ")" << std::endl;
    os << "Blending mode: " << blending_mode << " (" << gui->get_blending_mode_name(blending_mode) << ")" << std::endl;
    os << std::endl;
    os << "CLIP: " << current_clip->get_id() << std::endl;
    os << "  FPS: " << current_clip->get_playhead_fps() << std::endl;
    os << "  Playhead: " << current_clip->get_playhead() << std::endl;
    os << "  Writehead: " << current_clip->get_writehead() << "/" << current_clip->size() << std::endl;
    os << "  Direction: " << Clip::get_direction_name(current_clip->get_direction()) << std::endl;
    os << std::endl;
    os << "  Intervalometer rate: " << current_clip->get_intervalometer_rate() << std::endl;
    os << "  Intervalometer enabled: " << (app_->get_pipeline()->get_intervalometer_is_on() ? "yes" : "no") << std::endl;
    clutter_text_set_text(CLUTTER_TEXT(text_), os.str().c_str());
}
