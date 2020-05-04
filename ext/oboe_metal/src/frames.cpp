#include "frames.h"
#include <sstream>


int Frames::extract_frame_info(VALUE frame, frame_t *frame_info) {
    // PROFILE_FUNCTION();
    VALUE val;

// TODO revert adding tid to class
    pid_t tid = AO_GETTID;
    std::stringstream tid_s;
    tid_s << (long)tid;

    val = rb_profile_frame_classpath(frame);  // returns class or nil
    if (RB_TYPE_P(val, T_STRING)) frame_info->klass = tid_s.str() + ' ' + RSTRING_PTR(val);

    val = rb_profile_frame_absolute_path(frame);  // returns file, use rb_profile_frame_path() if nil
    if (!RB_TYPE_P(val, T_STRING)) val = rb_profile_frame_path(frame); 
    if (RB_TYPE_P(val, T_STRING)) frame_info->file = RSTRING_PTR(val);

    val = rb_profile_frame_label(frame);  // returns method or block
    if (RB_TYPE_P(val, T_STRING)) frame_info->method = RSTRING_PTR(val);

    val = rb_profile_frame_first_lineno(frame); // returns line number
    if (RB_TYPE_P(val, T_FIXNUM)) frame_info->lineno = NUM2INT(val);

    return (frame_info->method.rfind("block ", 0) != 0);
}

// helper function to print frame from ruby pointers to frame
void Frames::print_raw_frame_info(VALUE frame) {
    VALUE val;
    int lineno;
    string file, klass, method;

    val = rb_profile_frame_path(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << "\nrb_profile_frame_path: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_absolute_path(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << ", rb_profile_frame_absolute_path: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_label(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << "\n, rb_profile_frame_label: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_base_label(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << ", rb_profile_frame_base_label: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_full_label(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << ", rb_profile_frame_full_label: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_classpath(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << "\n, rb_profile_frame_classpath: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_method_name(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << "\n, rb_profile_frame_method_name: " << RSTRING_PTR(val); 
    // val = rb_profile_frame_qualified_method_name(frame);
    // if (RB_TYPE_P(val, T_STRING)) std::cout << ", rb_profile_frame_qualified_method_name: " << RSTRING_PTR(val); 

    val = rb_profile_frame_first_lineno(frame); // returns line number
    if (RB_TYPE_P(val, T_FIXNUM)) lineno = NUM2INT(val);

   val = rb_profile_frame_classpath(frame);  // returns class or nil
    if (RB_TYPE_P(val, T_STRING)) klass = RSTRING_PTR(val);

    val = rb_profile_frame_absolute_path(frame);  // returns file, use rb_profile_frame_path() if nil
    if (!RB_TYPE_P(val, T_STRING)) val = rb_profile_frame_path(frame); 
    if (RB_TYPE_P(val, T_STRING)) file = RSTRING_PTR(val);

    val = rb_profile_frame_label(frame);  // returns method or block
    if (RB_TYPE_P(val, T_STRING)) method = RSTRING_PTR(val);

    cout << "   "
         << lineno << " "
         << file << " "
         << klass << " "
         << method << endl;
}

// helper function to print frame info
void Frames::print_frame_info(frame_t *frame, int i) {
    std::cout << i << ": "
              << frame->lineno << " "
              << frame->file << " "
              << frame->klass << " "
              << frame->method << std::endl;
}

/////
// in-place removal of 
// - frames with line number == 0
// - all but last of repeated frames
int Snapshot::remove_garbage(VALUE *frames_buffer, int num) {
    // PROFILE_FUNCTION();
    // 1) ignore top frames where the line number is 0
    bool go = true;
    while(go) {
        VALUE val = rb_profile_frame_first_lineno(frames_buffer[num - 1]);
        go = (!RB_TYPE_P(val, T_FIXNUM) || !NUM2INT(val));
        if (go) num--;
    }

    // 2) remove all repeated frames, keep the last one
    int count = 0;
    int k = 0;
    bool found = false;
    while (count < num-k) {
        // is this frame repeated ahead? 
        // if so we will replace it with the next one in line
    	for(int j = count+k+1; j < num; j++) {
    	  if (frames_buffer[count] == frames_buffer[j]) {
    	  	found = true;
    	  	break;
    	  }
    	}

    	if(found){
            // if we found this frame again later in the snapshot
            // we are going to override this one
            // but not if this is going beyond the boundary
    		k++;
            if(count+k < num-1) frames_buffer[count] = frames_buffer[count+k];
        } else {
    		count++;
    		frames_buffer[count] = frames_buffer[count+k];
    	}
    	found = false;
    }

    // 3) remove "block" frames, they are reported inconsistently and mess up
    //    the profile in the dashboard
    num = count;
    count = 0, k = 0;
    VALUE val;
    string method, file;
    while(count < num-k) {
       frames_buffer[count] = frames_buffer[count+k];
       val = rb_profile_frame_label(frames_buffer[count]);  // returns method or block
       // get the method or use block if its not readable
       method = RB_TYPE_P(val, T_STRING) ? RSTRING_PTR(val) : "block ";

    //    val = rb_profile_frame_absolute_path(frames_buffer[count]);  // returns file, use rb_profile_frame_path() if nil
    //    if (!RB_TYPE_P(val, T_STRING)) val = rb_profile_frame_path(frames_buffer[count]);
    //    file = RB_TYPE_P(val, T_STRING) ? RSTRING_PTR(val) : "_";

    //    if (method.rfind("block ", 0) == 0 ||
    //        (!app_root.empty() && file.rfind(app_root, 0) != 0)) {
       if (method.rfind("block ", 0) == 0) {
           k++;
       } else {
           count++;
       }
    }

    return count;
 }

// returns the number of the matching frames
int Snapshot::compare(VALUE *frames_buffer,      int num,
                      VALUE *prev_frames_buffer, int prev_num) {

    // PROFILE_FUNCTION();
    int i;
    int min = std::min(num, prev_num);
    
    for(i = 0; i < min; i++) {
        if (frames_buffer[num - 1 - i] != prev_frames_buffer[prev_num - 1 - i]) {
            return i;
        }
    }

    return i;
}
