gaim::signal connect [gaim::account handle] account-away { account state message } {
	gaim::debug -info "tcl signal" "account-away [gaim::account username $account] \"$state\" \"$message\""
}

gaim::signal connect [gaim::account handle] account-connecting { account } {
	gaim::debug -info "tcl signal" "account-connecting [gaim::account username $account]"
}

gaim::signal connect [gaim::account handle] account-set-info { account info } {
	gaim::debug -info "tcl signal" "account-set-info [gaim::account username $account] $info"
}

gaim::signal connect [gaim::account handle] account-setting-info { account info } {
	gaim::debug -info "tcl signal" "account-set-info [gaim::account username $account] $info"
}

gaim::signal connect [gaim::buddy handle] buddy-away { buddy } {
	gaim::debug -info "tcl signal" "buddy-away [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::buddy handle] buddy-back { buddy } {
	gaim::debug -info "tcl signal" "buddy-back [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::buddy handle] buddy-idle { buddy } {
	gaim::debug -info "tcl signal" "buddy-idle [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::buddy handle] buddy-unidle { buddy } {
	gaim::debug -info "tcl signal" "buddy-unidle [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::buddy handle] buddy-signed-on { buddy } {
	gaim::debug -info "tcl signal" "buddy-signed-on [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::buddy handle] buddy-signed-off { buddy } {
	gaim::debug -info "tcl signal" "buddy-signed-off [gaim::account username [lindex $buddy 2]] [lindex $buddy 1]"
}

gaim::signal connect [gaim::core handle] quitting {} {
	gaim::debug -info "tcl signal" "quitting"
}

gaim::signal connect [gaim::conversation handle] receiving-chat-msg { account who what id flags } {
	gaim::debug -info "tcl signal" "receiving-chat-msg [gaim::account username $account] $id $flags $who \"$what\""
	return 0
}

gaim::signal connect [gaim::conversation handle] receiving-im-msg { account who what id flags } {
	gaim::debug -info "tcl signal" "receiving-im-msg [gaim::account username $account] $id $flags $who \"$what\""
	return 0
}

gaim::signal connect [gaim::conversation handle] received-chat-msg { account who what id flags } {
	gaim::debug -info "tcl signal" "received-chat-msg [gaim::account username $account] $id $flags $who \"$what\""
}

gaim::signal connect [gaim::conversation handle] received-im-msg { account who what id flags } {
	gaim::debug -info "tcl signal" "received-im-msg [gaim::account username $account] $id $flags $who \"$what\""
}

gaim::signal connect [gaim::conversation handle] sending-chat-msg { account what id } {
	gaim::debug -info "tcl signal" "sending-chat-msg [gaim::account username $account] $id \"$what\""
	return 0
}

gaim::signal connect [gaim::conversation handle] sending-im-msg { account who what } {
	gaim::debug -info "tcl signal" "sending-im-msg [gaim::account username $account] $who \"$what\""
	return 0
}

gaim::signal connect [gaim::conversation handle] sent-chat-msg { account id what } {
	gaim::debug -info "tcl signal" "sent-chat-msg [gaim::account username $account] $id \"$what\""
}

gaim::signal connect [gaim::conversation handle] sent-im-msg { account who what } {
	gaim::debug -info "tcl signal" "sent-im-msg [gaim::account username $account] $who \"$what\""
}

gaim::signal connect [gaim::connection handle] signed-on { gc } {
	gaim::debug -info "tcl signal" "signed-on [gaim::account username [gaim::connection account $gc]]"
}

gaim::signal connect [gaim::connection handle] signed-off { gc } {
	gaim::debug -info "tcl signal" "signed-off [gaim::account username [gaim::connection account $gc]]"
}

gaim::signal connect [gaim::connection handle] signing-on { gc } {
	gaim::debug -info "tcl signal" "signing-on [gaim::account username [gaim::connection account $gc]]"
}

if { 0 } {
gaim::signal connect signing-off {
	gaim::debug -info "tcl signal" "signing-off [gaim::account username [gaim::connection account $event::gc]]"
}

gaim::signal connect update-idle {
	gaim::debug -info "tcl signal" "update-idle"
}
}

proc plugin_init { } {
	list "Tcl Signal Test" \
             "$gaim::version" \
	     "Tests Tcl signal handlers" \
             "Debugs a ridiculous amount of signal information." \
             "Ethan Blanton <eblanton@cs.purdue.edu>" \
             "http://gaim.sourceforge.net/"
}
