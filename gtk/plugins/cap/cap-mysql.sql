-- Contact Availability Prediction plugin for Gaim
--
-- Copyright (C) 2006 Geoffrey Foster.
--
-- This program is free software; you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation; either version 2 of the
-- License, or (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
-- General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
-- 02111-1307, USA.

drop table if exists cap_status;
drop table if exists cap_message;
drop table if exists cap_msg_count;
drop table if exists cap_status_count;
drop table if exists cap_my_usage;

create table if not exists cap_status (
	buddy varchar(60) not null,
	account varchar(60) not null,
	protocol varchar(60) not null,
	status varchar(60) not null,
	event_time datetime not null,
	primary key (buddy, account, protocol, event_time)
);
	
create table if not exists cap_message (
	sender varchar(60) not null,
	receiver varchar(60) not null,
	account varchar(60) not null,
	protocol varchar(60) not null,
	word_count integer not null,
	event_time datetime not null,
	primary key (sender, account, protocol, receiver, event_time)
);

create table if not exists cap_msg_count (
	buddy varchar(60) not null,
	account varchar(60) not null,
	protocol varchar(60) not null,
	minute_val int not null,
	success_count int not null,
	failed_count int not null,
	primary key (buddy, account, protocol, minute_val)
);

create table if not exists cap_status_count (
	buddy varchar(60) not null,
	account varchar(60) not null,
	protocol varchar(60) not null,
	status varchar(60) not null,
	success_count int not null,
	failed_count int not null,
	primary key (buddy, account, protocol, status)
);

create table if not exists cap_my_usage (
	account varchar(60) not null,
	protocol varchar(60) not null,
	online tinyint not null,
	event_time datetime not null,
	primary key(account, protocol, online, event_time)
);
