# SPDX-License-Identifier: MPL-2.0
# Copyright (C) 2026 Styves Goupil
"""The River's emails: SMTP over TLS (Zoho: smtp.zoho.com:465; 587 is STARTTLS), sent after the response (a
background task). With no SMTP_HOST (a local run, the tests), written to the log instead, and kept in SENT."""
import logging
import smtplib
import ssl
from email.message import EmailMessage
from email.utils import formatdate, make_msgid

log = logging.getLogger("river.mail")
SENT = []                                         # (with no SMTP: the last ones, for the tests and a local run)


def send(s, to, subject, body):
    msg = EmailMessage()
    msg["From"], msg["To"], msg["Subject"] = s.mail_from, to, subject
    msg["Date"], msg["Message-ID"] = formatdate(localtime=False), make_msgid(domain="frasilsynth.com")
    if s.reply_to:
        msg["Reply-To"] = s.reply_to
    msg.set_content(body)
    if not s.smtp_host:
        SENT.append({"to": to, "subject": subject, "body": body})
        del SENT[:-50]
        log.warning("no SMTP_HOST: the email to %s, %r:\n%s", to, subject, body)
        return
    try:
        if s.smtp_port == 465:
            with smtplib.SMTP_SSL(s.smtp_host, 465, context=ssl.create_default_context(), timeout=20) as c:
                c.login(s.smtp_user, s.smtp_password)
                c.send_message(msg)
        else:
            with smtplib.SMTP(s.smtp_host, s.smtp_port, timeout=20) as c:
                c.starttls(context=ssl.create_default_context())
                c.login(s.smtp_user, s.smtp_password)
                c.send_message(msg)
    except (OSError, smtplib.SMTPException) as e:
        log.error("email to %s not sent: %s", to, e)


def sign(s):
    return f"\n\nFRASIL River\n{s.url}/\n" + (f"Questions: {s.reply_to}\n" if s.reply_to else "")


# (nothing a user typed goes into them: they reach addresses not yet proved)
def verify(s, to, link):
    send(s, to, "Confirm your email for FRASIL River",
         f"Hello,\n\nConfirm your email to share shards on FRASIL River:\n\n{link}\n\n"
         "This link works for 48 hours. If you didn't make an account on the River, ignore this email." + sign(s))


def reset(s, to, link):
    send(s, to, "Reset your FRASIL River password",
         f"Hello,\n\nTo choose a new password for FRASIL River, open:\n\n{link}\n\n"
         "This link works for 1 hour, once. If you didn't ask for it, ignore this email: your password stays as it "
         "is." + sign(s))
