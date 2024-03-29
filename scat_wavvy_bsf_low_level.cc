#include "scat_wavvy_bsf.h"


// virtual functions -  just to improve the readability of the cpp file.
void ScatFormWavvy::fire(Event * e)
{
    Scheduler & s = Scheduler::instance();
    BTNode* wk = node_;
    LMPLink* link ;


    // The commented part may be needed later
    if (e == &busyDelayEv_){
        if (trace_node()) {
            fprintf(stderr, "** %d BusyDelay Event is triggered at %f \n", id_, s.clock());
        }

        busyCond_ = false;            // is this right ? Yes.
        ex_round_messages();



    } else if (e == &waitingDiscEv_) {       // Rememeber to use the exact steps as in BlueStars; (i.e. pending_receiver for example);

        if (trace_node()) {
            fprintf(stderr, "********** %d fired waitingDiscEv_ with %d **********\n", id_, pending_receiver);
        }


        if (pending_receiver != -1) {       // this is just to prevent unexpected errors
            s.cancel(&waitingDiscEv_);
            _expectingDisconnection(pending_receiver);
            node_->bnep_->disconnect(pending_receiver,'a');
        }
        waitingDiscCounter_++ ;

        if (waitingDiscCounter_ >= 1) {
            if (trace_node()) {
                fprintf(stderr, "in %d waitingDiscCounter >= 1 - waitingDiscCounter %d \n", id_,waitingDiscCounter_ );
                fprintf(stdout, "in %d waitingDiscCounter >= 1 - waitingDiscCounter %d \n", id_,waitingDiscCounter_ );
            }


            // find the link and detach it - ALSO: we may check if it is detached from the other piconet
            link = node_->lmp_->lookupLink(pending_receiver);
            link->piconet->detach_link(link, 'a');

            if (trace_node()) {
                fprintf(stderr, "Node %d detached the link of %d at %f\n", id_, pending_receiver, s.clock());
            }

            // _exRoundMessages();    // no need cuz detach_link calls linkDetached() which call _exRoundMessages
            //abort();

        }


    } else if (e == &watchDogEv_ ) {
        // delete all links and then go to _exRoundMessages - reason ='e'


        fprintf(stderr, "WATCHDOG TIMER FIRED: %d @ %f with status = %s \n", id_, s.clock(), node_->bb_->state_str(node_->bb_->state()));
        abort();

        if (node_->bb_->state() != Baseband::PAGE_SCAN  ) {

            do {
                link = node_->lmp_->lookupLink(((ScatFormWavvy *) wk->scatFormator_)->id_);
                if (link) {


                    if (trace_node()) {
                        fprintf(stderr, "%d delete link %d which is at state = %s ", id_,
                                ((ScatFormWavvy *) wk->scatFormator_)->id_,
                                wk->bb_->state_str(wk->bb_->state()));

                        fprintf(stderr, "While node %d is in state %s \n",
                                id_,
                                node_->bb_->state_str(node_->bb_->state()));

                        fprintf(stdout, "%d delete link %d which is at state = %s \n",
                                id_,
                                ((ScatFormWavvy *) wk->scatFormator_)->id_,
                                wk->bb_->state_str(wk->bb_->state()));
                    }


                    // i think we will need to add a new if statement
                    if (node_->bb_->state() == Baseband::CONNECTION || wk->bb_->state() == Baseband::CONNECTION) {
                        // fprintf(stdout, "*** %d skipped the detach ! the state is CONNECTION at  %d \n ", id_);
                        //interruptedNode_ = ((ScatFormBlueMIS_2 *) wk->scatFormator_)->id_;
                        // s.schedule(&timer_ , &interruptionEv_ , INTERRUPTION_TIME );


                    } else {

                        link->piconet->detach_link(link, 'e');

                        if (trace_node()) {
                            fprintf(stderr, " ** the deletion is done ! \n");
                        }

                    }
                }
            } while ((wk = wk->getNext()) != node_);


            if (trace_node()) {
                fprintf(stderr, "we are before _exRoundmessages() \n");
            }
            ex_round_messages();

        }


    }
    return;
}
void ScatFormWavvy::linkDetached(bd_addr_t rmt, uchar reason) {

    Scheduler & s = Scheduler::instance();

    // This is used for "Busy" Messages
    if (rmt == pending_receiver && waitingDiscCond_ == true) {
        pending_receiver = -1;
        waitingDiscCond_ = false;
        s.cancel(&waitingDiscEv_);
    }

    if (destroy_temp_pico){
        if (busyCond_ == true) {

            // i.e. delay the execution of _exRoundMessages for BUSYDELAY time slots
            s.schedule(&timer_ , &busyDelayEv_ , BUSYDELAY );

        } else {

            if (node_->lmp_->curPico && !node_->lmp_->curPico->isMaster()) {
                if (( node_->bnep_->lookupConnection(rmt))) {
                    node_->bnep_->removeConnection(node_->bnep_->lookupConnection(rmt));
                }
            }

            // Note: this is an important function that I have ignored ..
            // This has been replaced by next() ..
            // _increaseCurrentPaging();

            // Let's test the impact of this function ..(i.e. ex_round_messages).
            // So this is a greatly important function .. if we don't have it,
            // then we would contact only one node during the whole algorithm.
            ex_round_messages();


        }
    }
}

void ScatFormWavvy::recv(Packet * p, int rmt) {

    Scheduler & s = Scheduler::instance();
    hdr_bt *bh = HDR_BT(p);
    SFmsg *msg = &bh->u.sf;
    int slot;

    if (msg->target != id_) {
        if ((slot = bnep_->findPortByIp(msg->target)) >= 0) {
            fprintf(stdout, "*** %f %d fwd to %d\n",
                    s.clock(), id_, msg->target);
            bnep_->_conn[slot]->cid->enque(p);
        } else {
            fprintf(stdout,
                    "*** %f %d Don't know where to send SFcmd to %d.",
                    s.clock(), id_, msg->target);
            Packet::free(p);
        }
        return;
    }

    assert(msg->type == type());

    if (trace_node()) {
        fprintf(stderr, "node %d is in %s and is going to recv_handler \n",  this->id_, __FUNCTION__);
    }
    recv_handler(msg, rmt);

}


void ScatFormWavvy::page_scan_completed() {
  Scheduler & s = Scheduler::instance();
  s.cancel(&watchDogEv_);
  s.schedule(&timer_ , &watchDogEv_ , WATCH_DOG_TIMER );
  fprintf(stdout, "%d is in page_scan_completed()\n", id_ );
}


void ScatFormWavvy::_pageNotSucced(int rmt) {
  // The lower layers do not notify that the connection was faild, and the higher
  // layers considers that it was established succefully; so we remove the connection
  Scheduler & s = Scheduler::instance();

  // Change the offset here;
  node_->bb_->recalculateNInfo(rmt, 1);

  // ALL THESE can be put into one function, cleanForConnection
  if ((node_->l2cap_->lookupChannel(PSM_BNEP, rmt))) {
    node_->l2cap_->removeChannel((node_->l2cap_->lookupChannel(PSM_BNEP, rmt)));
  } else {
    fprintf(stderr, "*** Error: channel is not there , no removeChannel() \n");
    abort();
  }

  if (( node_->bnep_->lookupConnection(rmt))) {
    node_->bnep_->removeConnection(node_->bnep_->lookupConnection(rmt));
  }

  fprintf(stderr, "**** Warning: %d PAGE was not succed with %d at %f \n",id_, rmt, s.clock());
  ex_round_messages();

}

/*
void ScatFormWavvy::_changeOffsetValue(int id) {
  BTNode *wk = node_->lookupNode(id);
  Bd_info *bd_temp;
  int my_clock = node_->bb_->clkn_;
  int clock, offset, slave_clock;

  // CHANGGING the offsets and the clocks
  if (!wk) {
    fprintf(stderr, "*** Error: THERE must be an error, NO node %d \n", id);
    abort();
  } else {
    bd_temp = node_->lmp_->lookupBdinfo(id);
    if (!bd_temp){
      fprintf(stderr, "*** Error: There must be an error , No neighbor %d \n", id);
      abort();

    } else {
      clock = bd_temp->clkn_;
      offset = bd_temp->offset_;

      slave_clock = (wk->bb_->clkn_ - 3) & 0xFFFFFFFC;
      bd_temp->clkn_ = slave_clock;
      bd_temp->offset_ = slave_clock -(my_clock & 0xFFFFFFFC);

    }
  }
}
*/
