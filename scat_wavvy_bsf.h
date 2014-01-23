#ifndef __SCAT_WAVVY_BSF__
#define __SCAT_WAVVY_BSF__

#include <vector>
#include <map>
#include <iostream>


#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "../bnep.h"
#include "../lmp.h"
#include "../lmp-link.h"
#include "../lmp-piconet.h"
#include "../l2cap.h"
#include "../scat-form.h"
#include "../baseband.h"
#include "../lmp.h"

#include "wavvy_neighbors.h"
#include "scat_wavvy_bsf_utilities.h"
using namespace std;


#define BUSYDELAY  0.05
#define WAITING_DISCONNECTION 0.25			// original (i.e. with collusion it was 0.25)
#define WATCH_DOG_TIMER 10.0					// original (i.e. with collusion it was 0.5)



class ScatFormWavvy;


struct MsgCandidate {

    MsgCandidate(node_id _id) : candidate_id(_id) { }
    MsgCandidate(node_id _id, bool _reply) : candidate_id(_id), reply(_reply) { }

    node_id candidate_id;
    bool reply;
};


// TODO: statuses .. need to be handled in a better way ..
// a class or a struct ..
class ScatFormWavvy : public ScatFormator {


    enum msgCmd {CmdCandidate, CmdForward, CmdResult};
    enum status {INIT,
                 UP_TO_DOWN_WAIT,
                 UP_TO_DOWN_ACTION,
                 DOWN_TO_UP_WAIT,
                 DOWN_TO_UP_ACTION,
                 TERMINATE};

    enum role {MASTER, SLAVE, NONE};
    enum node_type {INTERMEDIATE, SOURCE, SINK, ISOLATED};


    static const char *state_str(status st) {
        static const char *const str[] = {
            "INIT", "UP_TO_DOWN_WAIT", "UP_TO_DOWN_ACTION",
            "DOWN_TO_UP_WAIT", "DOWN_TO_UP_ACTION", "TERMINATE",
            "Invalid_state"
        };
        return str[st];
    }
    // This is unneessary repetition ..
    // inline const char *state_str() { return state_str(my_status); }


    struct msg_statistics {
        msg_statistics() : rcvd(0),  conns(0) {}
        int rcvd, conns;
    } msg_stats;


public:

    ScatFormWavvy(BTNode* n);

    //TODO there must be changes somwhere else ..
    inline Type type() { return SFWavvy; }
    virtual void start() { _main(); }
    virtual void fire(Event * e);
    virtual void connected(bd_addr_t rmt);
    virtual void linkDetached(bd_addr_t rmt, uchar reason);
    virtual void recv(Packet * p, int rmt);

    // it would have been better if this was virtual ..
    void _addNeighbor(node_id id);
    void _printResultsInFile(const char* filename);


protected:

    // initialization before main.
    void init_scat_formation_algorithm();

    void flip_neighbor_direction(node_id u, const char* code);
    void make_unnecessary_neighbor(node_id u, nvect& neighbors_list);
    void seperate_all_neighbors();

    void _main();
    void ex_round_messages();

    // DELETED .. done ..
    // bool is_all_contacted();

    // TODELETE
    void mark_neighbor_contacted(nvect& list, int n_id);

    void recv_handler(SFmsg* msg, int rmt);
    void recv_handler_cmd_candidate(SFmsg* msg, int rmt);

    // TOCHANGE ... done
    // bool _calculatePromising(int& ad);
    // node_id find_promising_receiver();

    // TODELETE .. .. done
    // void _increaseCurrentPaging();      // which neighbors should you contact now.
    // void _decreaseCurrentPaging();      // I think - we do not need these two functions.
    // No, actually .. we need this ..
    // What if the connection was not successful ..
    // either - we go to the next (using a global
    // current_paging counter) ..
    // but we can also .. random sampling!
    // This is our to-do.



    // These functions are made to avoid errors in communications..
    void _expectingDisconnection(int rmt);
    void reset();
    void _pageNotSucced(int rmt);
    void _changeOffsetValue(int id);


    // debugging functions.
    inline bool trace_node();
    void print_all(FILE*);
    void print_result_in_row(FILE*);

    // handling states;
    void change_status(status s);

    // handling connected:
    void initiate_connected_up_to_down_action(bd_addr_t rmt);
    void initiate_connected_down_to_up_action(bd_addr_t rmt);


private:

    // wait for, cancel, and make connection ..
    inline void wait_in_page_scan();
    inline void cancel_page_scan();
    inline void connect_page(int receiver_id);
    inline void disconnect_page(int receiver_id);

    //TODO this should be moved to a different header file. -- done ..
    // void move_node(node_id u, nvect* from, nvect* to);

    // TODELETE .. .. done
    //nvect_iter find_neighbor(nvect* vect, node_id u);
    //void clear_contact_info(nvect* list);


    node_type get_node_type() {
        int up_size = up_neighbors.size();
        int down_size = down_neighbors.size();

        if (up_size > 0 && down_size > 0) return INTERMEDIATE;
        if (up_size == 0 && down_size > 0) return SOURCE;
        if (up_size > 0 && down_size == 0) return SINK;
        if (up_size == 0 && down_size == 0) return ISOLATED;

        else {
            fprintf(stderr, "error in %d in function %s \n", this->id_, __FUNCTION__);
            abort();
        }
    }



private:
    nvect all_neighbors, up_neighbors, down_neighbors, unnecessary_neighbors;
    // node_id id_;
    status my_status;
    int current_iteration;
    role my_role;
    map<node_id, node_id> candidate_table;
    float finishing_time;

    // Handling busy states and connections:
    Event busyDelayEv_, waitingDiscEv_, watchDogEv_;
    bool busyCond_, waitingDiscCond_, destroy_temp_pico;
    node_id pending_receiver;
    int waitingDiscCounter_;


};




// Connection-related functions ..
inline void ScatFormWavvy::wait_in_page_scan() {
    // initiate the watchdog timer ..
    // go to page_scan state.

    Scheduler::instance().schedule(&timer_ , &watchDogEv_ , WATCH_DOG_TIMER );
    node_->lmp_->_page_scan(0);
}
inline void ScatFormWavvy::cancel_page_scan() {
    // cancel the page scan ..
    node_->lmp_->_bb_cancel();
}
inline void ScatFormWavvy::connect_page(int receiver_id) {
    Scheduler & s = Scheduler::instance();

    s.schedule(&timer_ , &watchDogEv_ , WATCH_DOG_TIMER );
    // fprintf(stderr, "watchdog timer of %d set at %s at %f \n", id_, __FUNCTION__, s.clock());

    node_->bnep_->connect(receiver_id);
    this->msg_stats.conns ++;

    // debugging ..
    if (trace_node()) {
        fprintf(stderr, "%d -> %d CON (%s) %f \n",
                id_, receiver_id,
                this->state_str(this->my_status),
                s.clock());
    }

}
inline void ScatFormWavvy::disconnect_page(int rmt) {

    // must be executed at the master of the link ..
    // break the link !
    _expectingDisconnection(rmt);
    node_->bnep_->disconnect(rmt, 'a');
}
inline void ScatFormWavvy::_expectingDisconnection(int rmt) {

    Scheduler & s = Scheduler::instance();
    pending_receiver = rmt;
    waitingDiscCond_ = true;
    s.schedule(&timer_ , &waitingDiscEv_ , WAITING_DISCONNECTION );

}




// We don't need this anymore ..
// TODO test me please ..
/* inline void ScatFormWavvy::mark_neighbor_contacted(nvect& list, int n_id) {
    nvect_iter it = list.find(n_id);
    if (it != list.end()) {
        it->second.contacted = true;
    }
} */






#endif
