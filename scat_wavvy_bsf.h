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


struct MsgResult {
    MsgResult(node_id _id, bool _yes_no, bool _reply) :
        max_candidate_id(_id),
        yes_no(_yes_no),
        reply(_reply) {}

    node_id max_candidate_id;
    bool yes_no;
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
    enum response_type {YES, NO, NOT_KNOWN};



    static const char *response2str(response_type t) {
        switch(t) {
        case YES: return "YES";
        case NO: return "NO";
        case NOT_KNOWN: return "NOT_KNOWN";
        default:
            fprintf(stderr, "unrecognized response_type .. ");
            abort();
        }


    }

    static const char *state_str(status st) {
        static const char *const str[] = {
            "INIT",
            "UP_TO_DOWN_WAIT",
            "UP_TO_DOWN_ACTION",
            "DOWN_TO_UP_WAIT",
            "DOWN_TO_UP_ACTION",
            "TERMINATE",
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

    void recv_handler(SFmsg* msg, int rmt);
    void recv_handler_cmd_candidate(SFmsg* msg, int rmt);
    void recv_handler_cmd_result(SFmsg* msg, int rmt);


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

    // honestly .. I m just trying to see if this appraoch is useful.
    // to better organize the code ..

    struct yes_no_map {
    public:


        yes_no_map(ScatFormWavvy* _scat_form_ptr):
            is_all_yes_(true),
            up_to_date(true),
            scat_form_ptr(_scat_form_ptr)
        {}

        void set_value(node_id rmt, response_type value) {
            table[rmt] = value;
            up_to_date = false;
        }


        // Note: I am trying to access id_ here .. let's see if it is in the
        // scope of the struct .. - well, if I don't inherit ScatWavvy, then I can't
        // because id_ is not a static variable.
        //
        response_type get_value(node_id rmt) {
            map<node_id, response_type>::iterator it;
            if ((it = table.find(rmt)) != table.end()) return it->second;

            fprintf(stderr, "error in %d trying to access an inexisting value yes_no_table[%d] \n",
                    scat_form_ptr->id_, rmt);
            abort();
        }

        void reset() {
            for (map<node_id, response_type>::iterator it = table.begin(); it != table.end();
                 it ++ ) it->second = NOT_KNOWN;
        }

        bool is_all_yes() {
            if (! up_to_date) calculate_all_yes();
            return is_all_yes_;
        }

        void print() {
            fprintf(stderr, "yes_no_t[%d]: ", scat_form_ptr->id_);
            for (map<node_id, response_type>::iterator it = table.begin(); it != table.end();
                 it ++ )
                fprintf(stderr, "(%d,%s) ", it->first, scat_form_ptr->response2str(it->second));
            fprintf(stderr, "\n");

        }

    private:
        bool is_all_yes_;
        bool up_to_date;
        map<node_id, response_type> table;
        ScatFormWavvy* scat_form_ptr;

        void calculate_all_yes() {
            // in the case of sinks ..
            fprintf(stderr, "I m in %s \n", __FUNCTION__);

            if (table.empty()) { up_to_date = true; is_all_yes_ = true;  return; }

            for (map<node_id, response_type>::iterator it = table.begin(); it != table.end();
                 it ++ ) {
                if (it->second != YES) { is_all_yes_ = false; up_to_date = true; return; }
            }
            is_all_yes_ = true;
            up_to_date = true;
            return;
        }

    };



private:
    nvect all_neighbors, up_neighbors, down_neighbors, unnecessary_neighbors;
    // node_id id_;
    status my_status;
    int current_iteration;
    role my_role;
    float finishing_time;

    map<node_id, node_id> candidate_table;
    yes_no_map yes_no_table;

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
