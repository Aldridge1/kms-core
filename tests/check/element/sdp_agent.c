/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#include "sdp_utils.h"
#include "kmssdpagent.h"
#include "kmssdpmediahandler.h"
#include "kmssdpsctpmediahandler.h"
#include "kmssdprtpavpmediahandler.h"
#include "kmssdprtpavpfmediahandler.h"
#include "kmssdprtpsavpfmediahandler.h"

static void
sdp_agent_create_offer (KmsSdpAgent * agent)
{
  GError *err = NULL;
  GstSDPMessage *offer;
  gchar *sdp_str;

  offer = kms_sdp_agent_create_offer (agent, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (offer);

  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  gst_sdp_message_free (offer);
}

GST_START_TEST (sdp_agent_test_create_offer)
{
  KmsSdpAgent *agent;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  sdp_agent_create_offer (agent);

  g_object_set (agent, "use-ipv6", TRUE, NULL);
  sdp_agent_create_offer (agent);

  g_object_unref (agent);
}

GST_END_TEST;

GST_START_TEST (sdp_agent_test_add_proto_handler)
{
  KmsSdpAgent *agent;
  KmsSdpMediaHandler *handler;
  gboolean ret;

  agent = kms_sdp_agent_new ();
  fail_if (agent == NULL);

  handler =
      KMS_SDP_MEDIA_HANDLER (g_object_new (KMS_TYPE_SDP_MEDIA_HANDLER, NULL));
  fail_if (handler == NULL);

  /* Try to add an invalid handler */
  ret = kms_sdp_agent_add_proto_handler (agent, "audio", handler);
  fail_if (ret);
  g_object_unref (handler);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (agent, "application", handler);
  fail_unless (ret);

  sdp_agent_create_offer (agent);

  g_object_unref (agent);
}

GST_END_TEST;

static const gchar *pattern_sdp_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendonly\r\n"
    "m=video 9 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=sendonly\r\n";

GST_START_TEST (sdp_agent_test_rejected_negotiation)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gboolean ret;
  gchar *sdp_str;
  guint i, len;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (answerer, "application", handler);
  fail_unless (ret);

  fail_unless (gst_sdp_message_new (&offer) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          pattern_sdp_str, -1, offer) == GST_SDP_OK);

  sdp_str = gst_sdp_message_as_text (offer);
  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  answer = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (answer);
  GST_DEBUG ("Answer:\n%s", sdp_str);
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (answer);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;

    media = gst_sdp_message_get_media (answer, i);
    fail_if (media == NULL);

    /* Media should have been rejected */
    fail_if (media->port != 0);
  }
  g_object_unref (answerer);
  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

GST_END_TEST;

static const gchar *pattern_sdp_sctp_str = "v=0\r\n"
    "o=- 0 0 IN IP4 0.0.0.0\r\n"
    "s=TestSession\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "t=2873397496 2873404696\r\n"
    "m=audio 9 RTP/AVP 0\r\n" "a=rtpmap:0 PCMU/8000\r\n" "a=sendonly\r\n"
    "m=video 9 RTP/AVP 96\r\n" "a=rtpmap:96 VP8/90000\r\n" "a=sendonly\r\n"
    "m=application 9 DTLS/SCTP 5000 5001 5002\r\n"
    "a=setup:actpass\r\n"
    "a=mid:data\r\n"
    "a=sctpmap:5000 webrtc-datachannel 1024\r\n"
    "a=sctpmap:5001 bfcp 2\r\n"
    "a=sctpmap:5002 t38 1\r\n"
    "a=webrtc-datachannel:5000 stream=1;label=\"channel 1\";subprotocol=\"chat\"\r\n"
    "a=webrtc-datachannel:5000 stream=2;label=\"channel 2\";subprotocol=\"file transfer\";max_retr=3\r\n"
    "a=bfcp:5000 stream=2;label=\"channel 2\";subprotocol=\"file transfer\";max_retr=3\r\n";

GST_START_TEST (sdp_agent_test_sctp_negotiation)
{
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  KmsSdpAgent *answerer;
  KmsSdpMediaHandler *handler;
  gboolean ret;
  gchar *sdp_str;
  guint i, len;

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_sctp_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (answerer, "application", handler);
  fail_unless (ret);

  fail_unless (gst_sdp_message_new (&offer) == GST_SDP_OK);
  fail_unless (gst_sdp_message_parse_buffer ((const guint8 *)
          pattern_sdp_sctp_str, -1, offer) == GST_SDP_OK);

  sdp_str = gst_sdp_message_as_text (offer);
  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  answer = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (answer);
  GST_DEBUG ("Answer:\n%s", sdp_str);
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  len = gst_sdp_message_medias_len (answer);

  for (i = 0; i < len; i++) {
    const GstSDPMedia *media;

    media = gst_sdp_message_get_media (answer, i);
    fail_if (media == NULL);

    /* Media should have been rejected */
    if (g_strcmp0 (gst_sdp_media_get_media (media), "application") != 0) {
      fail_if (media->port != 0);
      continue;
    } else {
      fail_if (media->port == 0);
    }

    /* This negotiation should only have 5 attributes */
    fail_if (gst_sdp_media_attributes_len (media) != 5);
  }
  g_object_unref (answerer);
  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
}

GST_END_TEST;

static gboolean
set_media_direction (GstSDPMedia * media, const gchar * direction)
{
  return gst_sdp_media_add_attribute (media, direction, "") == GST_SDP_OK;
}

static gboolean
expected_media_direction (const GstSDPMedia * media, const gchar * expected)
{
  guint i, attrs_len;

  attrs_len = gst_sdp_media_attributes_len (media);

  for (i = 0; i < attrs_len; i++) {
    const GstSDPAttribute *attr;

    attr = gst_sdp_media_get_attribute (media, i);

    if (g_ascii_strcasecmp (attr->key, expected) == 0) {
      return TRUE;
    }
  }

  return FALSE;
}

static void
negotiate_rtp_avp (const gchar * direction, const gchar * expected)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gboolean ret;
  gchar *sdp_str;
  const GstSDPMedia *media;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avp_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_unless (ret);

  /* re-use handler for audio */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_unless (ret);

  /* re-use handler for video in answerer */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_unless (ret);

  offer = kms_sdp_agent_create_offer (offerer, &err);

  fail_unless (sdp_utils_for_each_media (offer,
          (GstSDPMediaFunc) set_media_direction, (gpointer) direction));

  sdp_str = gst_sdp_message_as_text (offer);
  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  answer = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (answer);
  GST_DEBUG ("Answer:\n%s", sdp_str);
  g_free (sdp_str);

  /* Check only audio media */
  media = gst_sdp_message_get_media (answer, 1);
  fail_if (media == NULL);

  fail_if (!expected_media_direction (media, expected));

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_START_TEST (sdp_agent_test_rtp_avp_negotiation)
{
  negotiate_rtp_avp ("sendonly", "recvonly");
  negotiate_rtp_avp ("recvonly", "sendonly");
  negotiate_rtp_avp ("sendrecv", "sendrecv");
}

GST_END_TEST
GST_START_TEST (sdp_agent_test_rtp_avpf_negotiation)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gboolean ret;
  gchar *sdp_str;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_avpf_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_unless (ret);

  /* re-use handler for audio */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_unless (ret);

  /* re-use handler for video in answerer */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_unless (ret);

  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_unless (ret);

  offer = kms_sdp_agent_create_offer (offerer, &err);

  sdp_str = gst_sdp_message_as_text (offer);
  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  answer = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (answer);
  GST_DEBUG ("Answer:\n%s", sdp_str);
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST
GST_START_TEST (sdp_agent_test_rtp_savpf_negotiation)
{
  KmsSdpAgent *offerer, *answerer;
  KmsSdpMediaHandler *handler;
  GError *err = NULL;
  GstSDPMessage *offer, *answer;
  gboolean ret;
  gchar *sdp_str;

  offerer = kms_sdp_agent_new ();
  fail_if (offerer == NULL);

  answerer = kms_sdp_agent_new ();
  fail_if (answerer == NULL);

  handler = KMS_SDP_MEDIA_HANDLER (kms_sdp_rtp_savpf_media_handler_new ());
  fail_if (handler == NULL);

  ret = kms_sdp_agent_add_proto_handler (offerer, "video", handler);
  fail_unless (ret);

  /* re-use handler for audio */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (offerer, "audio", handler);
  fail_unless (ret);

  /* re-use handler for video in answerer */
  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (answerer, "video", handler);
  fail_unless (ret);

  g_object_ref (handler);
  ret = kms_sdp_agent_add_proto_handler (answerer, "audio", handler);
  fail_unless (ret);

  offer = kms_sdp_agent_create_offer (offerer, &err);

  sdp_str = gst_sdp_message_as_text (offer);
  GST_DEBUG ("Offer:\n%s", sdp_str);
  g_free (sdp_str);

  answer = kms_sdp_agent_create_answer (answerer, offer, &err);
  fail_if (err != NULL);

  sdp_str = gst_sdp_message_as_text (answer);
  GST_DEBUG ("Answer:\n%s", sdp_str);
  g_free (sdp_str);

  /* Same number of medias must be in answer */
  fail_if (gst_sdp_message_medias_len (offer) !=
      gst_sdp_message_medias_len (answer));

  gst_sdp_message_free (offer);
  gst_sdp_message_free (answer);
  g_object_unref (offerer);
  g_object_unref (answerer);
}

GST_END_TEST static Suite *
sdp_agent_suite (void)
{
  Suite *s = suite_create ("kmssdpagent");
  TCase *tc_chain = tcase_create ("SdpAgent");

  suite_add_tcase (s, tc_chain);

  tcase_add_test (tc_chain, sdp_agent_test_create_offer);
  tcase_add_test (tc_chain, sdp_agent_test_add_proto_handler);
  tcase_add_test (tc_chain, sdp_agent_test_rejected_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_sctp_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_avp_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_avpf_negotiation);
  tcase_add_test (tc_chain, sdp_agent_test_rtp_savpf_negotiation);

  return s;
}

GST_CHECK_MAIN (sdp_agent)
